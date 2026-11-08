/* Headless macOS runner for the SCI engine.
 * usage: scirun <gamedir> [-t trace] [-f frames_dir] [-n max_ticks] [-e script]
 * Script file lines: "<tick> click <x> <y>" or "<tick> key <code>". */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../src/engine/resource.h"
#include "../src/engine/gfx.h"
#include "../src/engine/vm.h"
#include "../src/engine/platform.h"
#include "../src/engine/music.h"
#include "../src/engine/voice.h"
#include "../src/engine/subtitle.h"
#include "../src/engine/balloon.h"
#include "../src/engine/topstatus.h"
#include "../src/engine/locpanel.h"

static uint32_t ticks;
static uint32_t max_ticks = 60 * 120;
static const char *frames_dir;
static int frame_no;
static int16_t mouse_x = 160, mouse_y = 100;

typedef struct { uint32_t tick; PlatEvent ev; } ScriptedEvent;
static ScriptedEvent events[256];
static int nevents, next_event;

void plat_vlog(const char *fmt, va_list ap) { vfprintf(stdout, fmt, ap); fflush(stdout); }

uint32_t plat_ticks(void)
{
    /* REALTIME=n: the clock also moves 1 tick per n reads, as on hardware */
    static int rt = -1, calls;
    if (rt < 0) rt = getenv("REALTIME") ? atoi(getenv("REALTIME")) : 0;
    if (rt && ++calls >= rt) { calls = 0; ticks++; }
    return ticks;
}

static void write_frame(void)
{
    if (!frames_dir) return;
    char path[512];
    snprintf(path, sizeof path, "%s/frame_%05d_t%06u.ppm", frames_dir, frame_no++, ticks);
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", SCREEN_W, SCREEN_H);
    static uint8_t frame[SCREEN_W * SCREEN_H];
    memcpy(frame, gfx.display, sizeof frame);
    balloon_overlay(frame, ticks);
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
        uint8_t ix = frame[i];
        PalColor *c = &gfx.palette[ix];
        int in = gfx.intensity[ix];
        fputc(c->r * in / 100, f); fputc(c->g * in / 100, f); fputc(c->b * in / 100, f);
    }
    fclose(f);
}

static FILE *wav;
static void wav_advance(int nticks);
static void voice_advance(int nticks);
static void wav_close(void);
static uint32_t last_frame_tick = 0xffffffff;
void plat_present(void)
{
    /* record at most one frame every 30 ticks to keep output manageable */
    if (last_frame_tick == 0xffffffff || ticks - last_frame_tick >= 30) {
        write_frame();
        last_frame_tick = ticks;
    }
}

void plat_sleep_ticks(int n)
{
    if (n < 1) n = 1;
    ticks += (uint32_t)n;
    if (wav) wav_advance(n); else voice_advance(n);
    { static int ta = -1; if (ta < 0) ta = getenv("TRACEAT") ? atoi(getenv("TRACEAT")) : 0;
      if (ta && ticks >= (uint32_t)ta && !vm.trace) vm.trace = getenv("TRACELVL") ? atoi(getenv("TRACELVL")) : 4; }
    if (ticks > max_ticks) {
        wav_close();
        if (getenv("TOPPNG")) {
            static uint8_t top[TOPSTATUS_W * TOPSTATUS_H];
            int r = topstatus_update(top, ticks);
            printf("topstatus: %d location %d\n", r, locpanel_active());
            FILE *f = fopen(getenv("TOPPNG"), "wb");
            fprintf(f, "P6\n%d %d\n255\n", TOPSTATUS_W, TOPSTATUS_H);
            for (int i = 0; i < TOPSTATUS_W * TOPSTATUS_H; i++) fwrite(topstatus_palette[top[i] % TOPSTATUS_COLORS], 1, 3, f);
            fclose(f);
        }
        if (getenv("INCAST")) {
            reg_t cast = vm_find_object("cast"), w = vm_find_object(getenv("INCAST"));
            reg_t list = cast ? get_sel(cast, sel_id("elements")) : 0;
            int found = 0;
            for (reg_t n = list ? hp16(list) : 0; n; n = hp16((uint16_t)(n + 2)))
                if (hp16((uint16_t)(n + 6)) == w) found = 1;
            printf("%s in cast: %d\n", getenv("INCAST"), found);
        }
        if (getenv("DUMPOBJ")) {
            char names[256]; strncpy(names, getenv("DUMPOBJ"), 255); names[255] = 0;
            for (char *t = strtok(names, ","); t; t = strtok(NULL, ",")) vm_dump_object(vm_find_object(t));
        }
        printf("reached max ticks %u (cache %u bytes, heap free %u)\n", max_ticks, res_cache_bytes(), heap_free_bytes());
        write_frame();
        exit(0);
    }
}

int plat_poll_event(PlatEvent *ev)
{
    /* each poll advances time a little so busy loops progress */
    static int polls;
    if (++polls % 4 == 0) plat_sleep_ticks(1);
    if (next_event < nevents && events[next_event].tick <= ticks) {
        *ev = events[next_event++].ev;
        mouse_x = ev->x; mouse_y = ev->y;
        printf("[t%u] inject event type %d msg %d at %d,%d\n", ticks, ev->type, ev->message, ev->x, ev->y);
        return 1;
    }
    return 0;
}

void plat_mouse(int16_t *x, int16_t *y) { *x = mouse_x; *y = mouse_y; }
void plat_set_cursor(const uint8_t *v, int type, int loop, int cel, int visible) { (void)v; (void)type; (void)loop; (void)cel; (void)visible; }
/* optional audio capture: the game's music rendered to a WAV (-w) */
static uint32_t wav_samples;
static int snd_master = 15, snd_muted;
void plat_sound(int cmd, int a, int b, int c)
{
    if (!wav) return;
    uint16_t sz;
    const uint8_t *d;
    switch (cmd) {
    case 0: snd_master = a; break;
    case 1: snd_muted = a; break;
    case 8:
        d = res_load(RES_SOUND, a, &sz);
        if (d) { music_play(a, d, sz, (int16_t)b, c ? c : 127); res_unlock(RES_SOUND, a); }
        printf("[t%u] music play %d loop %d vol %d\n", ticks, a, (int16_t)b, c);
        return;
    case 9: music_stop(a); printf("[t%u] music stop %d\n", ticks, a); return;
    case 14: music_set_volume(a, b); return;
    }
    music_set_master(snd_muted ? 0 : snd_master);
}
/* CD speech */
static FILE *voice_vol;
int plat_voice_read(uint32_t off, void *buf, uint32_t size)
{
    return voice_vol && !fseek(voice_vol, (long)off, SEEK_SET) && fread(buf, 1, size, voice_vol) == size;
}
int plat_voice(int cmd, int arg)
{
    uint8_t *old = NULL;
    int r = 0;
    switch (cmd) {
    case 1: case 2: {
        uint8_t *b; uint32_t len;
        int f = voice_fetch(arg, &b, &len);
        r = f < 0 ? 0 : voice_install(arg, b, len, cmd == 2, &old);
    } printf("[t%u] voice %s %d len %d\n", ticks, cmd == 2 ? "play" : "load", arg, r); break;
    case 3: voice_stop(); break;
    case 4: voice_pause(1); break;
    case 5: voice_pause(0); break;
    case 6: r = voice_position(); break;
    case 8: voice_set_volume(arg); break;
    }
    free(old);
    return r;
}
static void voice_advance(int nticks)    /* keep clip position moving without -w */
{
    static int16_t buf[22050 / 60 + 2];
    for (int i = 0; i < nticks; i++) voice_mix(buf, 22050 / 60, 22050);
}
static void wav_advance(int nticks)
{
    static int16_t buf[22050 / 60 + 2];
    static uint32_t frac;
    for (int i = 0; i < nticks; i++) {
        frac += 22050;
        int n = (int)(frac / 60);
        frac %= 60;
        music_render(buf, n);
        voice_mix(buf, n, 22050);
        fwrite(buf, 2, n, wav);
        wav_samples += n;
    }
}
static void wav_close(void)
{
    if (!wav) return;
    uint32_t hdr[11] = { 0x46464952, 36 + wav_samples * 2, 0x45564157, 0x20746d66, 16,
                         1 | 1 << 16, 22050, 44100, 2 | 16 << 16, 0x61746164, wav_samples * 2 };
    fseek(wav, 0, SEEK_SET);
    fwrite(hdr, 4, 11, wav);
    fclose(wav);
    wav = NULL;
}
static const char *save_dir = ".";
int plat_save(int slot, const void *data, uint32_t size)
{
    char path[512];
    snprintf(path, sizeof path, "%s/jones%d.sav", save_dir, slot);
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    int ok = fwrite(data, 1, size, f) == size;
    return fclose(f) == 0 && ok;
}
int plat_load(int slot, void *data, uint32_t maxsize)
{
    char path[512];
    snprintf(path, sizeof path, "%s/jones%d.sav", save_dir, slot);
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    size_t n = fread(data, 1, maxsize, f);
    fclose(f);
    return (int)n;
}

static int file_read(void *user, uint32_t off, void *buf, uint32_t n)
{
    return !fseek((FILE *)user, (long)off, SEEK_SET) && fread(buf, 1, n, (FILE *)user) == n;
}

static uint8_t *slurp(const char *path, uint32_t *size)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc(n ? n : 1);
    if (fread(buf, 1, n, f) != (size_t)n) { free(buf); fclose(f); return NULL; }
    fclose(f);
    *size = (uint32_t)n;
    return buf;
}

static void load_events(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); return; }
    char line[256];
    while (fgets(line, sizeof line, f) && nevents < 254) {
        unsigned t; char kind[16]; int a = 0, b = 0;
        if (sscanf(line, "%u %15s %d %d", &t, kind, &a, &b) < 2) continue;
        if (!strcmp(kind, "click")) {
            events[nevents++] = (ScriptedEvent){ t, { EV_MOUSEDOWN, 0, 0, (int16_t)a, (int16_t)b } };
            events[nevents++] = (ScriptedEvent){ t + 2, { EV_MOUSEUP, 0, 0, (int16_t)a, (int16_t)b } };
        } else if (!strcmp(kind, "key")) {
            events[nevents++] = (ScriptedEvent){ t, { EV_KEYDOWN, (uint16_t)a, 0, mouse_x, mouse_y } };
        }
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s gamedir [-t n] [-f dir] [-n ticks] [-e events]\n", argv[0]); return 2; }
    int trace = 0;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "-t") && i + 1 < argc) trace = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-f") && i + 1 < argc) frames_dir = argv[++i];
        else if (!strcmp(argv[i], "-n") && i + 1 < argc) max_ticks = (uint32_t)atoi(argv[++i]);
        else if (!strcmp(argv[i], "-e") && i + 1 < argc) load_events(argv[++i]);
        else if (!strcmp(argv[i], "-s") && i + 1 < argc) save_dir = argv[++i];
        else if (!strcmp(argv[i], "-w") && i + 1 < argc) { wav = fopen(argv[++i], "wb"); if (wav) fseek(wav, 44, SEEK_SET); }
    }
    char path[1024];
    uint32_t msize;
    snprintf(path, sizeof path, "%s/resource.map", argv[1]);
    uint8_t *map = slurp(path, &msize);
    if (!map) { perror(path); return 1; }
    ResVolume vols[RES_MAX_VOLUMES];
    int nvols = 0;
    for (int i = 1; i <= RES_MAX_VOLUMES; i++) {
        snprintf(path, sizeof path, "%s/resource.%03d", argv[1], i);
        if (getenv("VOLREAD")) {           /* exercise the on-demand path */
            FILE *f = fopen(path, "rb");
            if (!f) break;
            fseek(f, 0, SEEK_END);
            vols[nvols] = (ResVolume){ NULL, (uint32_t)ftell(f), file_read, f };
            nvols++;
            continue;
        }
        uint8_t *d = slurp(path, &vols[nvols].size);
        if (!d) break;
        vols[nvols] = (ResVolume){ d, vols[nvols].size, NULL, NULL };
        nvols++;
    }
    res_init(map, msize, vols, nvols);
    res_set_cache_budget(1024 * 1024);   /* same as the DS build */
    snprintf(path, sizeof path, "%s/audio001.map", argv[1]);
    uint32_t amsize;
    uint8_t *amap = slurp(path, &amsize);
    if (amap) {
        snprintf(path, sizeof path, "%s/audio001.002", argv[1]);
        voice_vol = fopen(path, "rb");
        printf("speech: %d clips%s\n", voice_init(amap, amsize), voice_vol ? "" : " (no audio volume)");
        uint32_t ssize;
        uint8_t *subs = slurp(getenv("SUBTITLES") ? getenv("SUBTITLES") : "build/subtitles.bin", &ssize);
        if (subs) subtitle_init(subs, ssize);
    }
    gfx_init();
    if (wav) {
        uint16_t psz;
        const uint8_t *patch = res_load(RES_PATCH, 3, &psz);
        music_init(22050, patch, psz);
    }
    if (vm_init() < 0) { printf("vm_init failed\n"); return 1; }
    vm.trace = trace;
    vm_run_game();
    wav_close();
    printf("game returned at tick %u (cache %u bytes)\n", ticks, res_cache_bytes());
    write_frame();
    return 0;
}
