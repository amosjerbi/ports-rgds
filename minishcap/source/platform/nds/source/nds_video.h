#pragma once
/* libnds-side video. Kept free of engine headers (their u32/s32 clash with libnds). */
#include <stdint.h>

void NdsVideo_Init(void);
/* Draws a w x h ABGR8888 frame (as produced by the software PPU) centred on the top
 * screen. pitch is in pixels. */
void NdsVideo_PresentTop(const uint32_t* abgr, int w, int h, int pitch);
/* Draws a w x h ABGR8888 frame on the bottom (touch) screen. */
void NdsVideo_PresentBottom(const uint32_t* abgr, int w, int h, int pitch);
