#include "libretro_host.h"

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

/* The few libretro definitions needed (libretro.h is MIT licensed; only the used subset is restated). */
#define RETRO_ENVIRONMENT_GET_CAN_DUPE 3
#define RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL 8
#define RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY 9
#define RETRO_ENVIRONMENT_SET_PIXEL_FORMAT 10
#define RETRO_ENVIRONMENT_GET_VARIABLE 15
#define RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE 17
#define RETRO_ENVIRONMENT_GET_LOG_INTERFACE 27
#define RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY 31
#define RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION 52
#define RETRO_PIXEL_FORMAT_XRGB8888 1
#define RETRO_DEVICE_JOYPAD 1
#define RETRO_MEMORY_SAVE_RAM 0
#define RETRO_MEMORY_SYSTEM_RAM 2

struct retro_game_info { const char* path; const void* data; size_t size; const char* meta; };
struct retro_system_av_info {
    struct { unsigned base_width, base_height, max_width, max_height; float aspect_ratio; } geometry;
    struct { double fps, sample_rate; } timing;
};
typedef bool (*env_t)(unsigned, void*);
typedef void (*video_t)(const void*, unsigned, unsigned, size_t);
typedef void (*audio_t)(int16_t, int16_t);
typedef size_t (*audio_batch_t)(const int16_t*, size_t);
typedef void (*poll_t)(void);
typedef int16_t (*input_t)(unsigned, unsigned, unsigned, unsigned);

static void* sLib;
static struct {
    void (*set_environment)(env_t);
    void (*set_video_refresh)(video_t);
    void (*set_audio_sample)(audio_t);
    void (*set_audio_sample_batch)(audio_batch_t);
    void (*set_input_poll)(poll_t);
    void (*set_input_state)(input_t);
    void (*init)(void);
    void (*deinit)(void);
    bool (*load_game)(const struct retro_game_info*);
    void (*unload_game)(void);
    void (*run)(void);
    void (*get_system_av_info)(struct retro_system_av_info*);
    void* (*get_memory_data)(unsigned);
    size_t (*get_memory_size)(unsigned);
    size_t (*serialize_size)(void);
    bool (*serialize)(void*, size_t);
} L;

static uint32_t sFrame[256 * 256];
static unsigned sPixFmt = 2; /* negotiated: 1 = XRGB8888, 2 = RGB565 (the core picks) */
static int sW = 160, sH = 144;
static uint32_t sButtons;
static void* sRom;
static unsigned long sFrames, sVideoCalls;
unsigned long Lr_FramesRun(void) { return sFrames; }
unsigned long Lr_VideoCalls(void) { return sVideoCalls; }

static void CoreLog(int level, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[core %d] ", level);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

static bool Env(unsigned cmd, void* data) {
    if (getenv("FROG_TRACE")) fprintf(stderr, "[lr] env %u\n", cmd);
    switch (cmd) {
        case RETRO_ENVIRONMENT_GET_CAN_DUPE: /* required by gambatte: a NULL frame means "repeat the last one" */
            *(bool*)data = true;
            return true;
        case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
            return true;
        case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
            *(void (**)(int, const char*, ...))data = CoreLog;
            return true;
        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
            unsigned f = *(unsigned*)data;
            if (f != RETRO_PIXEL_FORMAT_XRGB8888 && f != 2)
                return false;
            sPixFmt = f;
            return true;
        }
        case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
        case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
            *(const char**)data = "/tmp";
            return true;
        case RETRO_ENVIRONMENT_GET_VARIABLE:
            ((struct { const char* key; const char* value; }*)data)->value = NULL; /* core defaults */
            return false;
        case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
            *(bool*)data = false;
            return true;
        case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
            *(unsigned*)data = 0;
            return true;
        default:
            return false;
    }
}

static void Video(const void* data, unsigned w, unsigned h, size_t pitch) {
    sVideoCalls++;
    if (!data || w > 256 || h > 256)
        return;
    sW = (int)w;
    sH = (int)h;
    for (unsigned y = 0; y < h; y++) {
        if (sPixFmt == 2) {
            const uint16_t* src = (const uint16_t*)((const uint8_t*)data + y * pitch);
            for (unsigned x = 0; x < w; x++) {
                uint16_t p = src[x];
                uint32_t r = ((p >> 11) & 31) * 255 / 31, g = ((p >> 5) & 63) * 255 / 63, b = (p & 31) * 255 / 31;
                sFrame[y * w + x] = 0xFF000000u | (b << 16) | (g << 8) | r;
            }
        } else {
            const uint32_t* src = (const uint32_t*)((const uint8_t*)data + y * pitch);
            for (unsigned x = 0; x < w; x++) {
                uint32_t p = src[x]; /* 0x00RRGGBB -> 0xFFBBGGRR */
                sFrame[y * w + x] = 0xFF000000u | ((p & 0xFF) << 16) | (p & 0xFF00) | ((p >> 16) & 0xFF);
            }
        }
    }
}
static void Audio(int16_t l, int16_t r) { (void)l; (void)r; }
static size_t AudioBatch(const int16_t* d, size_t n) { (void)d; return n; }
static void Poll(void) {}
static int16_t Input(unsigned port, unsigned dev, unsigned idx, unsigned id) {
    (void)idx;
    return (port == 0 && dev == RETRO_DEVICE_JOYPAD && id < 16 && (sButtons & (1u << id))) ? 1 : 0;
}

#define SYM(field, name) do { *(void**)&L.field = dlsym(sLib, name); if (!L.field) { fprintf(stderr, "core lacks %s\n", name); return false; } } while (0)

bool Lr_Open(const char* corePath, const char* romPath) {
    sLib = dlopen(corePath, RTLD_NOW | RTLD_LOCAL);
    if (!sLib) {
        fprintf(stderr, "[lr] dlopen %s: %s\n", corePath, dlerror());
        return false;
    }
    SYM(set_environment, "retro_set_environment");
    SYM(set_video_refresh, "retro_set_video_refresh");
    SYM(set_audio_sample, "retro_set_audio_sample");
    SYM(set_audio_sample_batch, "retro_set_audio_sample_batch");
    SYM(set_input_poll, "retro_set_input_poll");
    SYM(set_input_state, "retro_set_input_state");
    SYM(init, "retro_init");
    SYM(deinit, "retro_deinit");
    SYM(load_game, "retro_load_game");
    SYM(unload_game, "retro_unload_game");
    SYM(run, "retro_run");
    SYM(get_system_av_info, "retro_get_system_av_info");
    SYM(get_memory_data, "retro_get_memory_data");
    SYM(get_memory_size, "retro_get_memory_size");
    SYM(serialize_size, "retro_serialize_size");
    SYM(serialize, "retro_serialize");
    L.set_environment(Env);
    L.set_video_refresh(Video);
    L.set_audio_sample(Audio);
    L.set_audio_sample_batch(AudioBatch);
    L.set_input_poll(Poll);
    L.set_input_state(Input);
    L.init();
    FILE* f = fopen(romPath, "rb");
    if (!f) {
        fprintf(stderr, "[lr] cannot open ROM %s\n", romPath);
        return false;
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    sRom = malloc((size_t)n);
    if (!sRom || fread(sRom, 1, (size_t)n, f) != (size_t)n) {
        fclose(f);
        return false;
    }
    fclose(f);
    struct retro_game_info info = { romPath, sRom, (size_t)n, NULL };
    if (!L.load_game(&info)) {
        fprintf(stderr, "[lr] core refused the ROM\n");
        return false;
    }
    return true;
}

void Lr_Close(void) {
    if (sLib) {
        L.unload_game();
        L.deinit();
        dlclose(sLib);
        sLib = NULL;
    }
    free(sRom);
    sRom = NULL;
}

void Lr_Run(int frames) {
    while (frames-- > 0) {
        L.run();
        sFrames++;
    }
}

void Lr_Press(int button, int hold, int after) {
    sButtons |= 1u << button;
    Lr_Run(hold);
    sButtons &= ~(1u << button);
    Lr_Run(after);
}

const uint32_t* Lr_Frame(int* w, int* h) {
    if (w) *w = sW;
    if (h) *h = sH;
    return sFrame;
}

uint8_t* Lr_Wram(size_t* size) {
    if (size) *size = L.get_memory_size(RETRO_MEMORY_SYSTEM_RAM);
    return L.get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
}

uint8_t* Lr_Sram(size_t* size) {
    if (size) *size = L.get_memory_size(RETRO_MEMORY_SAVE_RAM);
    return L.get_memory_data(RETRO_MEMORY_SAVE_RAM);
}

bool Lr_SaveState(const char* path) {
    size_t n = L.serialize_size();
    void* buf = malloc(n);
    bool ok = buf && L.serialize(buf, n);
    if (ok) {
        FILE* f = fopen(path, "wb");
        ok = f && fwrite(buf, 1, n, f) == n;
        if (f)
            fclose(f);
    }
    free(buf);
    return ok;
}
