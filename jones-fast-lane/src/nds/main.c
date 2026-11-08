/* Jones in the Fast Lane - NDS port: platform layer + entry point.
 *
 * Bottom screen (touch): the whole 320x200 game scaled to 256x192 by the
 *                        hardware (no CPU cost).
 *                        Tapping = mouse click at that point.
 * Top screen:            off until a game is under way, then the players'
 *                        life goals.
 * A = Enter, B = Esc, D-pad = arrow keys, START = Enter.
 * L = F5 (save game), R = F7 (restore game).
 * Saves go to /data/jones/ on the SD card (DLDI flashcart or DSi SD).
 */
#include <nds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <strings.h>
#include <fat.h>
#include "../engine/resource.h"
#include "../engine/gfx.h"
#include "../engine/vm.h"
#include "../engine/platform.h"
#include "../engine/music.h"
#include "../engine/topstatus.h"
#include "../engine/voice.h"
#include "../engine/subtitle.h"
#include "../engine/balloon.h"
#include <calico/nds/nitrorom.h>

extern const uint8_t res_map_data[], res_map_end[];
#ifndef CDROM
extern const uint8_t res_vol1_data[], res_vol1_end[];
extern const uint8_t res_vol2_data[], res_vol2_end[];
#endif

static int bg_top, bg_bot;
static volatile uint32_t vbl_ticks;
static int16_t mouse_x = 160, mouse_y = 100;

static void vblank_handler(void) { vbl_ticks++; }

#ifdef PERF
/* Profile build: timers 2+3 cascade = 32-bit counter at 33.5 MHz. */
static u32 perf_now(void)
{
    u16 hi, lo;
    do { hi = REG_TMxCNT_L(3); lo = REG_TMxCNT_L(2); } while (hi != REG_TMxCNT_L(3));
    return (u32)hi << 16 | lo;
}
static u32 perf_music, perf_present, perf_sleep, perf_frames, perf_start;
static void perf_init(void)
{
    REG_TMxCNT_H(2) = 0; REG_TMxCNT_H(3) = 0; REG_TMxCNT_L(2) = 0; REG_TMxCNT_L(3) = 0;
    REG_TMxCNT_H(3) = TIMER_CASCADE | TIMER_ENABLE; REG_TMxCNT_H(2) = TIMER_ENABLE;
    perf_start = perf_now();
}
static void perf_report(void)
{
    u32 tot = perf_now() - perf_start;
    if (tot < 33513982u * 5) return;                     /* every 5 s */
    extern uint32_t kperf_waits, kperf_wait_arg;
    vm_log("w%lu/%lu ", (unsigned long)kperf_waits, (unsigned long)kperf_wait_arg); kperf_waits = 0;
    vm_log("t%lu fr %lu mus %lu%% pres %lu%% sleep %lu%%\n", (unsigned long)vbl_ticks, (unsigned long)perf_frames,
           (unsigned long)(perf_music / (tot / 100)), (unsigned long)(perf_present / (tot / 100)),
           (unsigned long)(perf_sleep / (tot / 100)));
    perf_music = perf_present = perf_sleep = perf_frames = 0;
    perf_start = perf_now();
}
#define PERF_BEGIN() u32 perf_t = perf_now()
#define PERF_END(acc) ((acc) += perf_now() - perf_t)
#else
#define PERF_BEGIN() ((void)0)
#define PERF_END(acc) ((void)0)
#endif

/* ------------------------------------------------------------- log */

static char logbuf[4096];
static int logpos;

void plat_vlog(const char *fmt, va_list ap)
{
    char line[160];
    int n = vsnprintf(line, sizeof line, fmt, ap);
    if (n <= 0) return;
    if (n > (int)sizeof line - 1) n = sizeof line - 1;
    if (logpos + n >= (int)sizeof logbuf) logpos = 0;
    memcpy(logbuf + logpos, line, n);
    logpos += n;
    logbuf[logpos] = 0;
}

/* ------------------------------------------------------------- time */

uint32_t plat_ticks(void) { return vbl_ticks; }

#ifdef AUTOPLAY
static volatile int ap_peak, ap_count;
#endif
static int top_status;   /* top screen shows the life-goals panel */
static void present_flush(void);

/* Top screen stays dark until a game is under way (then: life goals). */
static void top_screen_off(void)
{
    bgHide(bg_top);
    bgUpdate();
    BG_PALETTE[0] = 0;          /* backdrop: black */
    powerOff(PM_BACKLIGHT_TOP);
}

void plat_sleep_ticks(int n)
{
    if (n <= 0) return;
    PERF_BEGIN();
    uint32_t until = vbl_ticks + (uint32_t)n;
    while ((int32_t)(until - vbl_ticks) > 0) {
        if (!pmMainLoop()) { vm.quit = 1; return; }
        threadWaitForVBlank();
        present_flush();
    }
    PERF_END(perf_sleep);
}

/* ------------------------------------------------------------- video */

static uint8_t status_buf[TOPSTATUS_W * TOPSTATUS_H] __attribute__((aligned(32)));

/* The game can present far more often than the 60 Hz display (the profile
 * build measured ~147 per second, ~46% of the CPU). Copy at most one frame
 * per vblank; a later present in the same frame is sent at the next one. */
static uint32_t present_tick = 0xffffffff;
static int present_pending;
static void present_now(void);

void plat_present(void)
{
    if (vbl_ticks == present_tick) { present_pending = 1; return; }
    present_now();
}

static void present_flush(void)
{
    if (present_pending && vbl_ticks != present_tick) present_now();
}

static void present_now(void)
{
    present_tick = vbl_ticks;
    present_pending = 0;
#ifdef PERF
    perf_frames++;
    perf_report();
#endif
    PERF_BEGIN();
    int st = topstatus_update(status_buf, vbl_ticks);
    if (st && !top_status) {                 /* switch top screen to the panel */
        top_status = 1;
        for (int i = 0; i < TOPSTATUS_COLORS; i++)
            BG_PALETTE[i] = RGB15(topstatus_palette[i][0] >> 3, topstatus_palette[i][1] >> 3, topstatus_palette[i][2] >> 3);
        bgSetScroll(bg_top, 0, 0);
        bgShow(bg_top);
        bgUpdate();
        powerOn(PM_BACKLIGHT_TOP);
        st = 2;
    } else if (!st && top_status) {          /* no game in progress: top screen off */
        top_status = 0;
        top_screen_off();
    }
    if (gfx.palette_dirty) {
        for (int i = 0; i < 256; i++) {
            PalColor *c = &gfx.palette[i];
            int in = gfx.intensity[i];
            u16 v = RGB15((c->r * in / 100) >> 3, (c->g * in / 100) >> 3, (c->b * in / 100) >> 3);
            BG_PALETTE_SUB[i] = v;
        }
        gfx.palette_dirty = 0;
    }
    DC_FlushRange(gfx.display, sizeof gfx.display);
    u16 *t = bgGetGfxPtr(bg_top), *b = bgGetGfxPtr(bg_bot);
    if (top_status) {
        if (st == 2) {
            DC_FlushRange(status_buf, sizeof status_buf);
            for (int y = 0; y < TOPSTATUS_H; y++)
                dmaCopy(status_buf + y * TOPSTATUS_W, t + y * 256, TOPSTATUS_W);
        }
    }
    const uint8_t *src = gfx.display;
    if (balloon_signature(plat_ticks())) {      /* speech balloon over a copy */
        static uint8_t frame[SCREEN_W * SCREEN_H] __attribute__((aligned(32)));
        memcpy(frame, gfx.display, sizeof frame);
        if (balloon_overlay(frame, plat_ticks())) { DC_FlushRange(frame, sizeof frame); src = frame; }
    }
    for (int y = 0; y < SCREEN_H; y++)
        dmaCopy(src + y * SCREEN_W, b + y * 256, SCREEN_W);
    PERF_END(perf_present);
}

void plat_set_cursor(const uint8_t *c, int type, int loop, int cel, int visible)
{
    (void)c; (void)type; (void)loop; (void)cel; (void)visible;
}

/* ------------------------------------------------------------- input */

static int touching;
static PlatEvent queue[16];
static int qhead, qtail;

static void push_event(uint16_t type, uint16_t msg)
{
    int next = (qtail + 1) & 15;
    if (next == qhead) return;
    queue[qtail] = (PlatEvent){ type, msg, 0, mouse_x, mouse_y };
    qtail = next;
}

static void poll_input(void)
{
    scanKeys();
    u32 down = keysDown(), held = keysHeld();
    if (held & KEY_TOUCH) {
        TouchData t;
        touchRead(&t);
        mouse_x = (int16_t)(t.px * SCREEN_W / 256);
        mouse_y = (int16_t)(t.py * SCREEN_H / 192);
        if (!touching) push_event(EV_MOUSEDOWN, 0);
        touching = 1;
    } else if (touching) {
        touching = 0;
        push_event(EV_MOUSEUP, 0);
    }
    if (down & (KEY_A | KEY_START)) push_event(EV_KEYDOWN, 13);
    if (down & KEY_B) push_event(EV_KEYDOWN, 27);
    if (down & KEY_UP) push_event(EV_KEYDOWN, 0x4800);
    if (down & KEY_DOWN) push_event(EV_KEYDOWN, 0x5000);
    if (down & KEY_LEFT) push_event(EV_KEYDOWN, 0x4b00);
    if (down & KEY_RIGHT) push_event(EV_KEYDOWN, 0x4d00);
    if (down & KEY_L) push_event(EV_KEYDOWN, 0x3f00);   /* F5: save */
    if (down & KEY_R) push_event(EV_KEYDOWN, 0x4100);   /* F7: restore */
}

int plat_poll_event(PlatEvent *ev)
{
    static uint32_t last_scan = 0xffffffff;
    if (vbl_ticks != last_scan) {
        last_scan = vbl_ticks;
        poll_input();
        present_flush();
    }
#ifdef AUTOPLAY
    {   /* test build: scripted taps (tick, x, y in game coordinates) */
        static const uint16_t taps[][3] = {
#ifdef CDROM
            { 200, 160, 100 }, { 4400, 160, 71 }, { 5200, 100, 110 }, { 5900, 90, 157 },
            { 6700, 232, 157 }, { 7900, 195, 137 }, { 10100, 25, 112 },
#else
            { 200, 160, 100 }, { 3500, 160, 71 }, { 4300, 100, 110 }, { 5000, 90, 157 },
            { 5800, 232, 157 }, { 7000, 195, 137 }, { 9200, 25, 112 },
#endif
        };
        static unsigned k, up;
        if (vbl_ticks > 10700) { vm_log("cache %u voice peak %d samples %d\n", res_cache_bytes(), ap_peak, ap_count); vm.quit = 1; }
        if (k < sizeof taps / sizeof *taps && vbl_ticks >= taps[k][0] + (up ? 2u : 0u)) {
            mouse_x = (int16_t)taps[k][1]; mouse_y = (int16_t)taps[k][2];
            push_event(up ? EV_MOUSEUP : EV_MOUSEDOWN, 0);
            if (up) k++;
            up = !up;
        }
    }
#endif
    if (qhead == qtail) return 0;
    *ev = queue[qhead];
    qhead = (qhead + 1) & 15;
    return 1;
}

void plat_mouse(int16_t *x, int16_t *y) { *x = mouse_x; *y = mouse_y; }

/* ------------------------------------------------------------- stubs */

/* ------------------------------------------------------------- audio */
/* The synth (engine/music.c) renders into a ring buffer that one hardware
 * channel loops over; a music thread keeps it filled ahead of the play
 * position, which is derived from calico's system tick counter. */

#define AUDIO_RATE 22050
#define RING       4096                 /* samples (power of two) */
#define AHEAD      2048                 /* samples kept rendered ahead */

static int16_t ring[RING] __attribute__((aligned(32)));
static Mutex audio_mutex;
static int audio_ok;
static unsigned audio_period;           /* sound clock cycles per sample */
static u32 audio_written;               /* samples rendered since start */
static int muted_flag, master_vol = 15;

static Thread music_thread;
static u8 music_stack[16 * 1024] __attribute__((aligned(8)));

/* Timer 0 overflows once per output sample (sound clock = system clock/2),
 * timer 1 counts the overflows; widened to 32 bits here. */
static u32 audio_played(void)
{
    static u16 last;
    static u32 high;
    u16 now = REG_TMxCNT_L(1);
    if (now < last) high += 0x10000;
    last = now;
    return high | now;
}

static int music_main(void *arg)
{
    (void)arg;
    for (;;) {
        threadWaitForVBlank();
        u32 played = audio_played();
        if (audio_written < played) audio_written = played;   /* fell behind: skip */
        /* Render at most ~1.5 frames of audio per wake-up. This thread outranks
         * the engine, so if the synth can't keep up (slow emulator) an
         * unbounded loop would starve the game forever; this way the sound
         * drops out but the game keeps running. */
        u32 budget = audio_written + AUDIO_RATE / 40;
        while (audio_written < played + AHEAD && audio_written < budget) {
            u32 pos = audio_written & (RING - 1);
            u32 n = RING - pos;
            if (n > played + AHEAD - audio_written) n = played + AHEAD - audio_written;
            if (n > budget - audio_written) n = budget - audio_written;
            PERF_BEGIN();
            mutexLock(&audio_mutex);
            music_render(ring + pos, (int)n);
            voice_mix(ring + pos, (int)n, AUDIO_RATE);
#ifdef AUTOPLAY
            { extern int voice_position(void); static int dummy; (void)dummy;
              if (voice_position() > 0) for (u32 i = 0; i < n; i++) { int v = ring[pos + i] < 0 ? -ring[pos + i] : ring[pos + i]; if (v > ap_peak) ap_peak = v; ap_count++; } }
#endif
            mutexUnlock(&audio_mutex);
            PERF_END(perf_music);
            DC_FlushRange(ring + pos, n * 2);
            audio_written += n;
        }
    }
    return 0;
}

static void audio_init(void)
{
    uint16_t psz;
    const uint8_t *patch = res_load(RES_PATCH, 3, &psz);
    if (!music_init(AUDIO_RATE, patch, psz)) { vm_log("audio: no AdLib patches\n"); return; }
    res_unlock(RES_PATCH, 3);
    soundInit();
    soundPowerOn();
    soundSetMixerVolume(127);
    audio_period = soundTimerFromHz(AUDIO_RATE);
    memset(ring, 0, sizeof ring);
    DC_FlushRange(ring, sizeof ring);
    soundPreparePcm(0 | SOUND_START, 2047, 64, audio_period, SoundMode_Repeat, SoundFmt_Pcm16,
                    ring, 0, sizeof ring / 4);
    REG_TMxCNT_H(0) = 0;
    REG_TMxCNT_H(1) = 0;
    REG_TMxCNT_L(0) = (u16)(0x10000 - 2 * audio_period);
    REG_TMxCNT_L(1) = 0;
    REG_TMxCNT_H(1) = TIMER_CASCADE | TIMER_ENABLE;
    REG_TMxCNT_H(0) = TIMER_ENABLE;          /* prescaler 1 */
    audio_written = AHEAD / 2;
    threadPrepare(&music_thread, music_main, NULL, &music_stack[sizeof music_stack], MAIN_THREAD_PRIO);
#ifndef NOMUSICTHREAD
    threadStart(&music_thread);
#endif
    audio_ok = 1;
}

void plat_sound(int cmd, int a, int b, int c)
{
    if (!audio_ok) return;
    switch (cmd) {
    case 0: master_vol = a; break;                       /* master volume */
    case 1: muted_flag = a; break;                       /* mute */
    case 8: {                                            /* play song a, loop b, volume c */
        uint16_t sz;
        const uint8_t *d = res_load(RES_SOUND, a, &sz);
        if (!d) return;
        mutexLock(&audio_mutex);
        music_play(a, d, sz, (int16_t)b, c ? c : 127);
        mutexUnlock(&audio_mutex);
        res_unlock(RES_SOUND, a);
        return;
    }
    case 9:                                              /* stop song a */
        mutexLock(&audio_mutex);
        music_stop(a);
        mutexUnlock(&audio_mutex);
        return;
    case 14:                                             /* song a volume b */
        mutexLock(&audio_mutex);
        music_set_volume(a, b);
        mutexUnlock(&audio_mutex);
        return;
    default: return;
    }
    mutexLock(&audio_mutex);
    music_set_master(muted_flag ? 0 : master_vol);
    mutexUnlock(&audio_mutex);
}

/* BOOTDIAG builds: the bottom backdrop colour shows how far startup got. */
#ifdef BOOTDIAG
#define DIAG(r,g,b) (BG_PALETTE_SUB[0] = RGB15(r,g,b))
#define DIAGW(r,g,b) do { for (int _i = 0; _i < 5; _i++) threadWaitForVBlank(); DIAG(r,g,b); } while (0)
#else
#define DIAG(r,g,b) ((void)0)
#define DIAGW(r,g,b) ((void)0)
#endif

/* ------------------------------------------------------------- speech */
/* CD build: audio001.map / audio001.002 live in the ROM's NitroFS. */

static int fat_ready(void);
static NitroRom *nitro;
static int voice_file = -1;

int plat_voice_read(uint32_t off, void *buf, uint32_t size)
{
    if (!nitro || voice_file < 0) return 0;
    /* in chunks: some loaders mishandle very large single card reads */
    for (uint32_t done = 0; done < size;) {
        uint32_t n = size - done > 16384 ? 16384 : size - done;
        if (!nitroromReadFile(nitro, (u16)voice_file, off + done, (uint8_t *)buf + done, n)) return 0;
        done += n;
    }
    return 1;
}

int plat_voice(int cmd, int arg)
{
    if (!audio_ok) return cmd == 6 ? -1 : 0;
    uint8_t *old = NULL, *buf = NULL;
    uint32_t len = 0;
    int r = 0;
    if ((cmd == 1 || cmd == 2) && voice_fetch(arg, &buf, &len) < 0) return 0;
    mutexLock(&audio_mutex);
    switch (cmd) {
    case 1: case 2: r = voice_install(arg, buf, len, cmd == 2, &old); break;
    case 3: voice_stop(); break;
    case 4: voice_pause(1); break;
    case 5: voice_pause(0); break;
    case 6: r = voice_position(); break;
    case 8: voice_set_volume(arg); break;
    }
    mutexUnlock(&audio_mutex);
    free(old);
#ifdef AUTOPLAY
    if (cmd == 1 || cmd == 2 || cmd == 3) vm_log("v%d,%d=%d t%lu ", cmd, arg, r, (unsigned long)vbl_ticks);
#endif
    return r;
}

#ifdef CDROM
static int cd_volume = -1;

static int nitro_read(void *user, uint32_t off, void *buf, uint32_t n)
{
    (void)user;
    return nitroromReadFile(nitro, (u16)cd_volume, off, buf, n);
}

/* Fallback when calico can't find its own ROM (no argv[0] and no readable
 * card, e.g. DraStic): look for a .nds on the SD card whose header matches
 * the one the loader left in memory, and read NitroFS from that file. */
static NitroRom nitro_file;
static FILE *nitro_fp;

static bool fp_read(void *user, u32 off, void *buf, u32 size)
{
    FILE *f = user;
    return fseek(f, (long)off, SEEK_SET) == 0 && fread(buf, 1, size, f) == size;
}
static void fp_close(void *user) { fclose(user); }
static const NitroRomIface fp_iface = { fp_read, fp_close };

static NitroRom *nitro_find_self(void)
{
    static const char *dirs[] = { "/", "/roms/nds/", "/nds/", NULL };
    const EnvNdsHeader *me = g_envAppNdsHeader;
    char path[300];
    for (int d = 0; dirs[d]; d++) {
        DIR *dp = opendir(dirs[d]);
        if (!dp) continue;
        struct dirent *e;
        while ((e = readdir(dp))) {
            size_t n = strlen(e->d_name);
            if (n < 5 || strcasecmp(e->d_name + n - 4, ".nds")) continue;
            snprintf(path, sizeof path, "%s%s", dirs[d], e->d_name);
            FILE *f = fopen(path, "rb");
            if (!f) continue;
            EnvNdsHeader h;
            if (fread(&h, 1, sizeof h, f) == sizeof h &&
                !memcmp(h.title, me->title, sizeof h.title) &&
                h.fat_rom_offset == me->fat_rom_offset && h.fnt_rom_offset == me->fnt_rom_offset &&
                h.arm9_size == me->arm9_size && h.header_crc16 == me->header_crc16) {
                NitroRomParams p = { me->fat_rom_offset, me->fat_size, me->fnt_rom_offset, me->fnt_size, 0 };
                if (nitroromOpen(&nitro_file, &p, &fp_iface, f)) {
                    closedir(dp);
                    nitro_fp = f;
                    vm_log("NitroFS: %s\n", path);
                    return &nitro_file;
                }
            }
            fclose(f);
        }
        closedir(dp);
    }
    return NULL;
}

/* The CD build keeps resource.001 and the speech files in NitroFS. */
static int nitro_init(void)
{
    /* Card reads first: mounting the SD card can be slow or, with a broken
     * emulator SD image, hang, and the game must still boot without it. */
    DIAG(8,0,8);
    DIAGW(31,0,0);                     /* a vblank wait wakes this thread */
    nitro = nitroromGetSelf();
    DIAGW(31,31,0);
    if (!nitro && fat_ready()) {       /* nitroromGetSelf may open argv[0] via stdio */
        nitro = nitroromGetSelf();
        if (!nitro) nitro = nitro_find_self();
    }
    DIAGW(0,31,0);
    if (!nitro) { vm_log("NitroFS not found: can't read game data\n"); return 0; }
    cd_volume = nitroromResolvePath(nitro, NITROROM_ROOT_DIR, "resource.001");
    if (cd_volume < 0) { vm_log("resource.001 missing from NitroFS\n"); return 0; }
    return 1;
}

static void speech_init(void)
{
    int m = nitroromResolvePath(nitro, NITROROM_ROOT_DIR, "audio001.map");
    voice_file = nitroromResolvePath(nitro, NITROROM_ROOT_DIR, "audio001.002");
    if (m < 0 || voice_file < 0) { vm_log("speech: files missing\n"); voice_file = -1; return; }
    uint32_t sz = nitroromGetFileSize(nitro, (u16)m);
    uint8_t *map = malloc(sz);
    if (map && nitroromReadFile(nitro, (u16)m, 0, map, sz))
        vm_log("speech: %d clips\n", voice_init(map, sz));
    free(map);
    int sf = nitroromResolvePath(nitro, NITROROM_ROOT_DIR, "subtitles.bin");
    if (sf >= 0) {
        uint32_t ssz = nitroromGetFileSize(nitro, (u16)sf);
        uint8_t *subs = malloc(ssz);          /* kept for the whole session */
        if (subs && nitroromReadFile(nitro, (u16)sf, 0, subs, ssz)) subtitle_init(subs, ssz);
    }
}
#endif

/* ------------------------------------------------------------- saves */

static int fat_state;   /* 0 untried, 1 mounted, -1 unavailable */

static int fat_ready(void)
{
    if (!fat_state) {
        fat_state = fatInitDefault() ? 1 : -1;
        vm_log("storage: %s\n", fat_state > 0 ? "SD card" : "none (saving disabled)");
    }
    return fat_state > 0;
}

static void save_path(char *buf, size_t n, int slot)
{
    snprintf(buf, n, "/data/jones/jones%d.sav", slot);
}

int plat_save(int slot, const void *data, uint32_t size)
{
    if (!fat_ready()) return 0;
    mkdir("/data", 0777);
    mkdir("/data/jones", 0777);
    char path[64], tmp[72];
    save_path(path, sizeof path, slot);
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    if (!f) return 0;
    int ok = fwrite(data, 1, size, f) == size;
    ok = (fclose(f) == 0) && ok;
    if (ok) { remove(path); ok = rename(tmp, path) == 0; }
    return ok;
}

int plat_load(int slot, void *data, uint32_t maxsize)
{
    if (!fat_ready()) return 0;
    char path[64];
    save_path(path, sizeof path, slot);
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    size_t n = fread(data, 1, maxsize, f);
    fclose(f);
    return (int)n;
}

/* ------------------------------------------------------------- main */

#ifdef SPINIDLE
/* Keeps calico's idle thread (which halts the CPU with CP15 wait-for-interrupt)
 * from ever running. Test for emulators that don't wake from that halt. */
static Thread spin_thread;
static u8 spin_stack[512] __attribute__((aligned(8)));
static int spin_main(void *arg) { (void)arg; for (;;) __asm__ volatile("" ::: "memory"); return 0; }
#endif

static Thread engine_thread;
static u8 engine_stack[128 * 1024] __attribute__((aligned(8)));

static int engine_main(void *arg)
{
    (void)arg;
#ifdef CDROM
    if (!nitro_init()) return 2;
    ResVolume vols[] = { { NULL, nitroromGetFileSize(nitro, (u16)cd_volume), nitro_read, NULL } };
#else
    ResVolume vols[] = {
        { res_vol1_data, (uint32_t)(res_vol1_end - res_vol1_data), NULL, NULL },
        { res_vol2_data, (uint32_t)(res_vol2_end - res_vol2_data), NULL, NULL },
    };
#endif
    res_init(res_map_data, (uint32_t)(res_map_end - res_map_data), vols, (int)(sizeof vols / sizeof *vols));
    res_set_cache_budget(1024 * 1024);
    DIAGW(0,31,31);
    gfx_init();
    audio_init();
    DIAGW(0,0,31);
#ifdef CDROM
    speech_init();
#endif
    DIAGW(31,0,31);
#ifdef VOICETEST
    {
        static uint8_t raw[64];
        int rd = plat_voice_read(0x200000, raw, sizeof raw);
        int sum = 0; for (unsigned i = 0; i < sizeof raw; i++) sum += raw[i];
        for (int i = 0; i < 60; i++) threadWaitForVBlank();
        int peak0 = 0;
        for (int i = 0; i < RING; i++) { int v = ring[i] < 0 ? -ring[i] : ring[i]; if (v > peak0) peak0 = v; }
        int len = plat_voice(2, 539);
        for (int i = 0; i < 40; i++) threadWaitForVBlank();
        int peak = 0;
        for (int i = 0; i < RING; i++) { int v = ring[i] < 0 ? -ring[i] : ring[i]; if (v > peak) peak = v; }
        vm_log("VOICETEST read %d sum %d len %d pos %d peak silent %d playing %d audio_ok %d\n",
               rd, sum, len, plat_voice(6, 0), peak0, peak, audio_ok);
        return 9;
    }
#endif
#ifdef AUDIOTEST
    {
        vm_log("audio_ok %d period %u\n", audio_ok, audio_period);
        DIAG(0,12,0);                                           /* dark green: before song */
        u32 c0 = audio_played();
        for (int i = 0; i < 30; i++) { threadWaitForVBlank(); DIAG(31 - i, 0, i); }   /* red->blue: frames pass */
        if (audio_played() == c0) { DIAG(31,16,0); for (;;) threadWaitForVBlank(); }  /* orange: clock stuck (silence) */
        DIAG(0,0,16);                                           /* dark blue: clock ok, starting song */
        plat_sound(8, 6, 1, 127);
        DIAG(16,0,0);                                           /* dark red: song started */
        u32 p0 = audio_played();
        for (int i = 0; i < 120; i++) threadWaitForVBlank();
        int peak = 0;
        for (int i = 0; i < RING; i++) { int v = ring[i] < 0 ? -ring[i] : ring[i]; if (v > peak) peak = v; }
        vm_log("played %lu->%lu written %lu\n", (unsigned long)p0, (unsigned long)audio_played(), (unsigned long)audio_written);
        vm_log("active ch %x peak %d\n", soundGetActiveChannels(), peak);
        if (audio_played() == p0) DIAG(31,16,0);                /* orange: play clock stuck */
        else if (!peak) DIAG(16,16,16);                         /* grey: clock ok, ring silent */
        else DIAG(0,31,0);                                      /* green: audio running */
        return 8;
    }
#endif
#ifdef SAVETEST
    {
        static char rb[32];
        int w = plat_save(9, "jones save test", 15);
        int r = plat_load(9, rb, sizeof rb);
        vm_log("SAVETEST write %d read %d '%s'\n", w, r, rb);
        return 7;
    }
#endif
    if (vm_init() < 0) return 1;
    DIAG(31,31,31);
    vm_run_game();
    return 0;
}

int main(void)
{
    videoSetMode(MODE_5_2D);
    videoSetModeSub(MODE_5_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankC(VRAM_C_SUB_BG);
    bg_top = bgInit(3, BgType_Bmp8, BgSize_B8_512x256, 0, 0);
    bg_bot = bgInitSub(3, BgType_Bmp8, BgSize_B8_512x256, 0, 0);
    /* bottom: whole game scaled to fit by the hardware (affine, 8.8) */
    bgSetScale(bg_bot, (320 << 8) / 256, (200 << 8) / 192);
    top_screen_off();
    bgUpdate();
#ifdef PERF
    perf_init();
#endif
    irqSet(IRQ_VBLANK, vblank_handler);
    irqEnable(IRQ_VBLANK);

#ifdef SPINIDLE
    threadPrepare(&spin_thread, spin_main, NULL, &spin_stack[sizeof spin_stack], THREAD_MIN_PRIO);
    threadStart(&spin_thread);
#endif
    threadPrepare(&engine_thread, engine_main, NULL, &engine_stack[sizeof engine_stack], MAIN_THREAD_PRIO + 1);
    threadAttachLocalStorage(&engine_thread, NULL);
    threadStart(&engine_thread);
    int rc = threadJoin(&engine_thread);

    /* game ended: show log on the top screen */
    powerOn(PM_BACKLIGHT_TOP);
    videoSetMode(MODE_0_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    consoleInit(NULL, 0, BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, true);
    printf("Game exited (rc %d)\n\n%s", rc, logbuf + (logpos > 900 ? logpos - 900 : 0));
    while (pmMainLoop()) threadWaitForVBlank();
    return 0;
}
