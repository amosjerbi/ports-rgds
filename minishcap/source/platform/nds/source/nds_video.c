#include "nds_video.h"

#include <nds.h>

#define SCREEN_W 256
#define SCREEN_H 192

static uint16_t* sTopGfx;
static uint16_t* sBottomGfx;

/* Main engine = gameplay (top screen); the sub engine keeps the text console that
 * also serves as the bottom-screen UI placeholder until the dual-screen UI lands. */
void NdsVideo_Init(void) {
    videoSetMode(MODE_5_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    int bg = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    sTopGfx = bgGetGfxPtr(bg);
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++)
        sTopGfx[i] = ARGB16(1, 0, 0, 0);
    lcdMainOnTop();

    /* Sub engine = second screen (map / quest / touch items), replacing the boot console. */
    videoSetModeSub(MODE_5_2D);
    vramSetBankC(VRAM_C_SUB_BG);
    int sbg = bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    sBottomGfx = bgGetGfxPtr(sbg);
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++)
        sBottomGfx[i] = ARGB16(1, 0, 0, 0);
}

static void Blit(uint16_t* gfx, const uint32_t* abgr, int w, int h, int pitch) {
    if (!gfx)
        return;
    if (w > SCREEN_W)
        w = SCREEN_W;
    if (h > SCREEN_H)
        h = SCREEN_H;
    const int x0 = (SCREEN_W - w) / 2, y0 = (SCREEN_H - h) / 2;
    for (int y = 0; y < h; y++) {
        const uint32_t* src = abgr + y * pitch;
        uint16_t* dst = gfx + (y0 + y) * SCREEN_W + x0;
        for (int x = 0; x < w; x++) {
            uint32_t p = src[x]; /* A B G R, R in the low byte */
            dst[x] = (uint16_t)(0x8000u | ((p & 0xF8u) >> 3) | ((p & 0xF800u) >> 6) | ((p & 0xF80000u) >> 9));
        }
    }
}

void NdsVideo_PresentBottom(const uint32_t* abgr, int w, int h, int pitch) {
    Blit(sBottomGfx, abgr, w, h, pitch);
}

void NdsVideo_PresentTop(const uint32_t* abgr, int w, int h, int pitch) {
    Blit(sTopGfx, abgr, w, h, pitch); /* 240x160 is centred in 256x192 */
}
