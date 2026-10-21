/* RG DS implementation of the Platform3DS_* API the shared engine consumes (see platform_3ds.h).
 * The engine is built with -DTMC_3DS -DTMC_RGDS: the maintained native-port paths, on a 64-bit target. */
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "display_rgds.h"
#include "platform_3ds.h"

#define GBA_FRAME_NS 16742706ull /* 280896 cycles / 16.78 MHz = 59.7275 Hz */

void Port_SecondScreen_3DS_OnTap(int x, int y, int longPress); /* engine side; plain C types */

static uint64_t sNextFrameNs;

static uint64_t NowNs(void) {
    /* counter * 1e9 overflows 64 bits when the counter is already in ns (Linux): split the division. */
    const uint64_t c = SDL_GetPerformanceCounter(), f = SDL_GetPerformanceFrequency();
    return (c / f) * 1000000000ull + (c % f) * 1000000000ull / f;
}

bool Platform3DS_IsRunning(void) {
    return RgdsDisplay_PumpEvents();
}

uint16_t Platform3DS_ReadKeyInput(void) {
    return RgdsDisplay_KeyInput();
}

uint16_t Platform3DS_ReadKeyDownInput(void) {
    return (uint16_t)RgdsDisplay_KeysDownMask();
}

uint32_t Platform3DS_KeysHeld(void) {
    return (uint32_t)(~RgdsDisplay_KeyInput() & 0x3FFu);
}

void Platform3DS_ReadCircle(float* x, float* y) {
    RgdsDisplay_LeftStick(x, y);
}

/* Frame pacing: hold the GBA's 59.7275 Hz. The top window is not vsynced, so this is the only throttle. */
void Platform3DS_WaitForVBlank(void) {
    const uint64_t now = NowNs();
    if (sNextFrameNs == 0 || now > sNextFrameNs + 4 * GBA_FRAME_NS)
        sNextFrameNs = now; /* first frame, or we fell far behind: resync instead of catching up */
    sNextFrameNs += GBA_FRAME_NS;
    if (sNextFrameNs > now)
        SDL_Delay((Uint32)((sNextFrameNs - now) / 1000000ull));
}

bool Platform3DS_BeginFrameBoundary(void) {
    static uint64_t windowStart;
    static unsigned frames;
    RgdsDisplay_PumpEvents();
    {   /* RGDS_TEST_TAP="frame,x,y": inject one tap at a given frame, for testing without a finger */
        static int tapFrame = -2, tapX, tapY, frameNo;
        if (tapFrame == -2) {
            const char* t = getenv("RGDS_TEST_TAP");
            tapFrame = (t && sscanf(t, "%d,%d,%d", &tapFrame, &tapX, &tapY) == 3) ? tapFrame : -1;
        }
        if (tapFrame >= 0 && ++frameNo == tapFrame) {
            fprintf(stderr, "[rgds] test tap at (%d,%d)\n", tapX, tapY);
            Port_SecondScreen_3DS_OnTap(tapX, tapY, 0);
        }
    }
    const uint64_t now = NowNs();
    if (windowStart == 0)
        windowStart = now;
    if (++frames == 120) { /* log the achieved engine frame rate every 120 frames */
        fprintf(stderr, "[rgds] %.1f fps\n", 120e9 / (double)(now - windowStart));
        windowStart = now;
        frames = 0;
    }
    return true;
}
void Platform3DS_EndFrameBoundary(void) {}
void Platform3DS_PumpWithoutVBlank(void) {
    RgdsDisplay_PumpEvents();
}
void Platform3DS_MarkFrameDiscontinuity(Old3DSFramePacerDiscontinuity reason) {
    (void)reason;
    sNextFrameNs = 0;
}

uint64_t Platform3DS_Milliseconds(void) {
    return NowNs() / 1000000ull;
}
uint64_t Platform3DS_SystemTick(void) {
    return NowNs() / 1000ull; /* microseconds: used only for relative timing */
}
uint64_t Platform3DS_TicksPerSecond(void) {
    return 1000000ull;
}

/* Native host pointers are always dereferenceable on a flat 64-bit address space. */
int Platform3DS_IsNativeAddress(uintptr_t v) {
    return v >= 0x10000u;
}
int Platform3DS_IsActiveStackAddress(uintptr_t v) {
    (void)v;
    return 0;
}

void Platform3DS_Debug(const char* message) {
    fputs(message, stderr);
}

void Platform3DS_ShowFatal(const char* title, const char* message) {
    fprintf(stderr, "\n[fatal] %s\n%s\n", title, message);
    exit(1);
}

/* port_rom.c allocates the ROM with libctru's linearAlloc under TMC_3DS. */
void* linearAlloc(size_t size) {
    return aligned_alloc(64, (size + 63u) & ~(size_t)63u);
}
void linearFree(void* p) {
    free(p);
}

const char* Rgds_DataDir(void) {
    static char dir[1024];
    if (!dir[0]) {
        const char* forced = getenv("RGDS_DATA_DIR");
        if (forced && *forced) {
            snprintf(dir, sizeof dir, "%s", forced);
        } else {
            ssize_t n = readlink("/proc/self/exe", dir, sizeof dir - 1);
            if (n > 0) {
                dir[n] = 0;
                char* slash = strrchr(dir, '/');
                if (slash)
                    *slash = 0;
            } else {
                snprintf(dir, sizeof dir, ".");
            }
        }
    }
    return dir;
}
