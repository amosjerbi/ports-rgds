#pragma once
/* SDL2 side of the RG DS port. Two windows, one per panel (DSI-2 top, DSI-1 bottom, 640x480 each).
 * Kept free of engine headers. */
#include <stdbool.h>
#include <stdint.h>

#define RGDS_PANEL_W 640
#define RGDS_PANEL_H 480

bool RgdsDisplay_Init(void);
/* Companion mode: only the bottom-panel window (the top panel belongs to the emulator). */
bool RgdsDisplay_InitBottomOnly(void);
void RgdsDisplay_Shutdown(void);
/* abgr: w x h pixels, R in the low byte. pitch is in pixels. */
void RgdsDisplay_PresentTop(const uint32_t* abgr, int w, int h, int pitch);
void RgdsDisplay_PresentBottom(const uint32_t* abgr, int w, int h, int pitch);
/* Pumps SDL events: input state, touch -> Port_SecondScreen_3DS_OnTap, quit. Returns false on quit. */
bool RgdsDisplay_PumpEvents(void);
uint16_t RgdsDisplay_KeyInput(void);   /* GBA KEYINPUT layout, active low */
uint32_t RgdsDisplay_KeysDownMask(void); /* edges since last call, GBA bit layout (active high) */
void RgdsDisplay_LeftStick(float* x, float* y);

/* Folder for saves and settings: the directory holding the executable (so the whole port can live in one
 * folder, e.g. /storage/roms/ports/tmc-rgds), or $RGDS_DATA_DIR. Returned string stays valid. */
const char* Rgds_DataDir(void);
