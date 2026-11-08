/* SaveGame / RestoreGame / CheckSaveGame / GetSaveFiles.
 *
 * A save file is a snapshot of the whole VM (64 KB heap + script/class
 * tables) plus the palette and the song table. Restoring unwinds the running
 * script and calls the game object's replay: method, as the DOS interpreter
 * does. Storage is up to the platform (plat_save / plat_load). */
#include <stdlib.h>
#include <string.h>
#include "kernel.h"
#include "gfx.h"

#define ARG(i)  hp16((uint16_t)(argv + 2 * (i)))

#define SAVE_MAGIC   0x5653434au   /* "JCSV" */
#define SAVE_FORMAT  3
#define DESC_LEN     36
#define MAX_SLOTS    20

typedef struct {
    uint32_t magic;
    uint16_t format, reserved;
    uint32_t total;
    char     game[16];
    char     version[16];
    char     desc[DESC_LEN];
} SaveHdr;

/* implemented in kgraphics.c / ksound.c */
void     kgfx_after_restore(void);
uint32_t ksound_state_size(void);
void     ksound_state_save(uint8_t *out);
void     ksound_state_load(const uint8_t *in);
void     ksound_after_restore(void);

static uint32_t pal_size(void) { return sizeof gfx.palette + sizeof gfx.intensity; }

static uint32_t save_max(void)
{
    return sizeof(SaveHdr) + pal_size() + ksound_state_size() + vm_state_size();
}

static void copy_str(char *dst, reg_t src, int n)
{
    memset(dst, 0, n);
    if (src) strncpy(dst, vm_string(src, 0, NULL), n - 1);
}

/* Reads slot into a fresh buffer; NULL if missing or not ours. */
static uint8_t *read_slot(int slot, const char *version)
{
    uint32_t max = save_max();
    uint8_t *buf = malloc(max);
    if (!buf) return NULL;
    SaveHdr h;
    int n = plat_load(slot, buf, max);
    if (n < (int)sizeof h) goto bad;
    memcpy(&h, buf, sizeof h);
    if (h.magic != SAVE_MAGIC || h.format != SAVE_FORMAT || h.total != (uint32_t)n) goto bad;
    if (version && strncmp(h.version, version, sizeof h.version - 1)) goto bad;
    return buf;
bad:
    free(buf);
    return NULL;
}

static int k_save(int argc, uint16_t argv)
{
    uint8_t *buf = malloc(save_max()), *p = buf;
    if (!buf) { vm_log("SaveGame: out of memory\n"); return 0; }
    SaveHdr h;
    memset(&h, 0, sizeof h);
    h.magic = SAVE_MAGIC;
    h.format = SAVE_FORMAT;
    copy_str(h.game, ARG(0), sizeof h.game);
    copy_str(h.desc, argc > 2 ? ARG(2) : 0, sizeof h.desc);
    copy_str(h.version, argc > 3 ? ARG(3) : 0, sizeof h.version);
    p += sizeof h;
    memcpy(p, gfx.palette, sizeof gfx.palette); p += sizeof gfx.palette;
    memcpy(p, gfx.intensity, sizeof gfx.intensity); p += sizeof gfx.intensity;
    ksound_state_save(p); p += ksound_state_size();
    p += vm_state_save(p);
    uint32_t sz = (uint32_t)(p - buf);
    h.total = sz;
    memcpy(buf, &h, sizeof h);
    int ok = plat_save(ARG(1), buf, sz);
    free(buf);
    vm_log("SaveGame slot %d '%s' (%u bytes): %s\n", ARG(1), h.desc, sz, ok ? "ok" : "FAILED");
    return ok;
}

static uint8_t *restore_pal_snd;   /* palette + sound part, applied after unwind */

static int k_restore(int argc, uint16_t argv)
{
    char version[16] = "";
    if (argc > 2) copy_str(version, ARG(2), sizeof version);
    uint8_t *buf = read_slot(ARG(1), argc > 2 ? version : NULL);
    if (!buf) { vm_log("RestoreGame slot %d: no valid save\n", ARG(1)); return 0; }
    uint32_t ps = pal_size() + ksound_state_size();
    SaveHdr h;
    memcpy(&h, buf, sizeof h);
    uint32_t vs = h.total - sizeof(SaveHdr) - ps;
    uint8_t *vmbuf = malloc(vs);
    free(restore_pal_snd);
    restore_pal_snd = malloc(ps);
    if (!vmbuf || !restore_pal_snd) {
        free(buf); free(vmbuf); free(restore_pal_snd); restore_pal_snd = NULL;
        return 0;
    }
    memcpy(restore_pal_snd, buf + sizeof(SaveHdr), ps);
    memcpy(vmbuf, buf + sizeof(SaveHdr) + ps, vs);
    free(buf);
    vm_log("RestoreGame slot %d\n", ARG(1));
    vm_request_restore(vmbuf);
    return 1;
}

/* called by vm_run_game once the heap has been replaced */
void kernel_after_restore(void)
{
    kernel_reset_disposals();   /* their scripts belong to the old heap */
    kgfx_after_restore();
    if (restore_pal_snd) {
        const uint8_t *p = restore_pal_snd;
        memcpy(gfx.palette, p, sizeof gfx.palette); p += sizeof gfx.palette;
        memcpy(gfx.intensity, p, sizeof gfx.intensity); p += sizeof gfx.intensity;
        gfx.palette_dirty = 1;
        ksound_state_load(p);
        free(restore_pal_snd);
        restore_pal_snd = NULL;
    }
    ksound_after_restore();
}

reg_t ksave_call(int num, int argc, uint16_t argv, int *handled)
{
    *handled = 1;
    switch (num) {
    case 0x29: return (reg_t)k_save(argc, argv);          /* SaveGame */
    case 0x2a: return (reg_t)k_restore(argc, argv);       /* RestoreGame */
    case 0x65: {                                          /* CheckSaveGame */
        char version[16] = "";
        if (argc > 2) copy_str(version, ARG(2), sizeof version);
        uint8_t *b = read_slot(ARG(1), argc > 2 ? version : NULL);
        free(b);
        return b != NULL;
    }
    case 0x5d: {                                          /* GetSaveFiles */
        uint16_t descs = ARG(1), ids = argc > 2 ? ARG(2) : 0;
        int n = 0;
        for (int slot = 0; slot < MAX_SLOTS; slot++) {
            uint8_t *b = read_slot(slot, NULL);
            if (!b) continue;
            SaveHdr h;
            memcpy(&h, b, sizeof h);
            free(b);
            memcpy(hstr((uint16_t)(descs + n * DESC_LEN)), h.desc, DESC_LEN);
            vm_heap[(uint16_t)(descs + n * DESC_LEN + DESC_LEN - 1)] = 0;
            if (ids) hw16((uint16_t)(ids + 2 * n), (reg_t)slot);
            n++;
        }
        vm_heap[(uint16_t)(descs + n * DESC_LEN)] = 0;
        if (ids) hw16((uint16_t)(ids + 2 * n), 0xffff);
        return (reg_t)n;
    }
    }
    *handled = 0;
    return 0;
}
