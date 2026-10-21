/* NDS implementation of the Platform3DS_* API consumed by the shared engine
 * (src/engine/src/main.c, src/port/port_bios.c, port_rom.c). The engine is built
 * with -DTMC_3DS -DTMC_NDS so it takes the 32-bit-pointer / no-SDL paths. */
#include <nds.h>
#include <stdio.h>
#include <string.h>

#include "platform_3ds.h"

bool Platform3DS_IsRunning(void) {
    return true;
}

/* DS key bits 0-9 (A,B,SELECT,START,RIGHT,LEFT,UP,DOWN,R,L) match GBA KEYINPUT,
 * which is active-low. */
uint16_t Platform3DS_ReadKeyInput(void) {
    return (uint16_t)(~keysHeld() & 0x3FFu);
}

void Platform3DS_ReadCircle(float* x, float* y) {
    if (x)
        *x = 0.0f;
    if (y)
        *y = 0.0f;
}

void Platform3DS_WaitForVBlank(void) {
    swiWaitForVBlank();
}

void Port_SecondScreen_3DS_OnTap(int x, int y, int longPress); /* engine-side; plain C types */

static void PollTouch(void) {
    scanKeys();
    if (keysDown() & KEY_TOUCH) {
        touchPosition t;
        touchRead(&t);
        Port_SecondScreen_3DS_OnTap(t.px, t.py, 0);
    }
}

bool Platform3DS_BeginFrameBoundary(void) {
    PollTouch();
    return true; /* always present */
}

void Platform3DS_EndFrameBoundary(void) {}

void Platform3DS_PumpWithoutVBlank(void) {
    PollTouch();
}

void Platform3DS_MarkFrameDiscontinuity(Old3DSFramePacerDiscontinuity reason) {
    (void)reason;
}

uint64_t Platform3DS_Milliseconds(void) {
    return (uint64_t)cpuGetTiming() * 1000u / BUS_CLOCK; /* needs cpuStartTiming(0) */
}

/* "Native" = host memory the engine may dereference: main RAM, ITCM/DTCM and the
 * Slot-2 ROM window (0x08000000..0x0A000000) that gRomData maps. */
int Platform3DS_IsNativeAddress(uintptr_t v) {
    return (v >= 0x02000000u && v < 0x02400000u) || (v >= 0x08000000u && v < 0x0A000000u) ||
           (v >= 0x01000000u && v < 0x01008000u) || (v >= 0x0B000000u && v < 0x0B004000u);
}

int Platform3DS_IsActiveStackAddress(uintptr_t v) {
    (void)v;
    return 0;
}

void Platform3DS_Debug(const char* message) {
    iprintf("%s\n", message);
}

void Platform3DS_ShowFatal(const char* title, const char* message) {
    iprintf("\n%s\n%s\n", title, message);
    for (;;)
        swiWaitForVBlank();
}
