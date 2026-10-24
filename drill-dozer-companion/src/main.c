/* dd-bottom: touch companion for Drill Dozer running in RetroArch's mGBA core on the RG DS.
 * Runs on the bottom panel (DSI-1). Reads / writes the game's chip counter over RetroArch's network command
 * interface (READ_CORE_MEMORY / WRITE_CORE_MEMORY, UDP 55355). Tabs: CHIPS, GUIDE. */
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dd_data.h"
#include "ra_client.h"

#define W 640
#define H 480
#define HDR_H 58
#define TAB_Y 424
#define TAB_H 56
#define MAX_SILENT_SECONDS 20
#define STARTUP_GRACE_SECONDS 180

#include "ui_common.h"

enum { TAB_CHIPS, TAB_GUIDE, TAB_COUNT };
static const char* kTabName[TAB_COUNT] = { "CHIPS", "GUIDE" };
enum { H_TAB = 100, H_STEP = 200 };
static const int kStep[] = { -1000, -100, -10, -1, 1, 10, 100, 1000 };
#define STEPS ((int)(sizeof kStep / sizeof kStep[0]))

static int sTab = TAB_CHIPS;
static bool sConnected, sValid;
static uint32_t sChips;
static char sToast[96];
static Uint32 sToastUntil;

static void Toast(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(sToast, sizeof sToast, fmt, ap);
    va_end(ap);
    sToastUntil = SDL_GetTicks() + 1800;
}

static bool PollState(void) {
    uint8_t b[4];
    if (!RaClient_Read(CHIPS_ADDR, b, 4))
        return false;
    sChips = (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
    sValid = sChips <= CHIPS_MAX;
    return true;
}

static void SetChips(int64_t v) {
    if (v < 0) v = 0;
    if (v > CHIPS_MAX) v = CHIPS_MAX;
    const uint8_t b[4] = { (uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24) };
    if (RaClient_Write(CHIPS_ADDR, b, 4)) {
        sChips = (uint32_t)v;
        Toast("Chips: %d", (int)v);
    } else {
        Toast("Write failed");
    }
}

static void DrawHeader(void) {
    FillRect(0, 0, W, HDR_H, C_PANEL);
    FillRect(0, HDR_H - 2, W, 2, C_GOLD);
    Text(sFont22, 14, 14, 0, C_GOLD, "DRILL DOZER");
    Text(sFont18, 626, 18, 2, C_CREAM, "Chips %u", sChips);
}

static void DrawChips(void) {
    Text(sFont14, W / 2, 84, 1, C_DIM, "Chips are the game's currency (Shop, Equipment screen)");
    FillRect(120, 120, 400, 90, C_PANEL);
    FrameRect(120, 120, 400, 90, C_GOLD);
    Text(sFont22, W / 2, 130, 1, C_GOLD, "CHIPS");
    Text(sFont22, W / 2, 164, 1, C_WHITE, "%05u", sChips);
    const int bw = 70, gap = 8, x0 = (W - STEPS * bw - (STEPS - 1) * gap) / 2;
    for (int i = 0; i < STEPS; i++) {
        const int x = x0 + i * (bw + gap), y = 240;
        FillRect(x, y, bw, 52, kStep[i] < 0 ? C_PANEL2 : C_RED);
        FrameRect(x, y, bw, 52, C_DIM);
        Text(sFont18, x + bw / 2, y + 15, 1, C_WHITE, "%+d", kStep[i]);
        AddHit(x, y, bw, 52, H_STEP + i);
    }
    FillRect(150, 312, 160, 52, C_PANEL2);
    FrameRect(150, 312, 160, 52, C_DIM);
    Text(sFont18, 230, 327, 1, C_CREAM, "ZERO");
    AddHit(150, 312, 160, 52, H_STEP + 20);
    FillRect(330, 312, 160, 52, C_RED);
    FrameRect(330, 312, 160, 52, C_GOLD);
    Text(sFont18, 410, 327, 1, C_WHITE, "MAX %d", CHIPS_MAX);
    AddHit(330, 312, 160, 52, H_STEP + 21);
}

static void DrawGuide(void) {
    static const char* const kLines[] = {
        "Hub menu (Red Dozer Trailer): Map, Equipment, Treasures, Save",
        "Equipment shows Drill Bit, Drill Type, Gear, Energy and Chips.",
        "Shop: Energy Tank (+100 max energy), Hard Bit, Mega Bit and the",
        "area maps (Hideout, Art Museum, Kuru Ruins, City, Clink Prison,",
        "Secret Lair). Pay with chips found in stages.",
        "Treasures: 31 in total, each hidden in a stage.",
        "In a stage the pause menu offers Save / Sleep / Exit / Back.",
    };
    for (int i = 0; i < (int)(sizeof kLines / sizeof kLines[0]); i++)
        Text(sFont14, 20, 80 + i * 28, 0, C_CREAM, "%s", kLines[i]);
}

static void DrawStatus(const char* l1, const char* l2) {
    FillRect(0, 0, W, H, C_BG);
    Text(sFont22, W / 2, 170, 1, C_GOLD, "DRILL DOZER");
    Text(sFont18, W / 2, 214, 1, C_CREAM, "%s", l1);
    Text(sFont14, W / 2, 246, 1, C_DIM, "%s", l2);
}

static void DrawTabs(void) {
    FillRect(0, TAB_Y - 2, W, H - TAB_Y + 2, C_PANEL);
    FillRect(0, TAB_Y - 2, W, 2, C_GOLD);
    const int tw = W / TAB_COUNT;
    for (int t = 0; t < TAB_COUNT; t++) {
        const bool act = t == sTab;
        FillRect(t * tw + 3, TAB_Y + 3, tw - 6, TAB_H - 6, act ? C_RED : C_PANEL2);
        FrameRect(t * tw + 3, TAB_Y + 3, tw - 6, TAB_H - 6, act ? C_GOLD : C_DIM);
        Text(sFont18, t * tw + tw / 2, TAB_Y + 17, 1, act ? C_WHITE : C_CREAM, "%s", kTabName[t]);
        AddHit(t * tw, TAB_Y, tw, TAB_H, H_TAB + t);
    }
}

static void DrawFrame(void) {
    SDL_SetRenderDrawColor(sRen, C_BG.r, C_BG.g, C_BG.b, 255);
    SDL_RenderClear(sRen);
    sHitCount = 0;
    if (!sConnected) {
        DrawStatus("Waiting for RetroArch...", "Launch Drill Dozer (mGBA core)");
    } else if (!sValid) {
        DrawStatus("Load a saved game", "The chip counter is not available yet");
    } else {
        DrawHeader();
        if (sTab == TAB_CHIPS) DrawChips(); else DrawGuide();
        DrawTabs();
    }
    if (SDL_GetTicks() < sToastUntil) {
        const int tw = TextWidth(sFont18, sToast) + 28;
        FillRect(W / 2 - tw / 2, 396, tw, 28, C_RED);
        FrameRect(W / 2 - tw / 2, 396, tw, 28, C_GOLD);
        Text(sFont18, W / 2, 399, 1, C_WHITE, "%s", sToast);
    }
    SDL_RenderPresent(sRen);
}

static void OnTap(int x, int y) {
    const int id = HitTest(x, y);
    if (id < 0)
        return;
    if (id >= H_TAB && id < H_TAB + TAB_COUNT)
        sTab = id - H_TAB;
    else if (id >= H_STEP && id < H_STEP + STEPS)
        SetChips((int64_t)sChips + kStep[id - H_STEP]);
    else if (id == H_STEP + 20)
        SetChips(0);
    else if (id == H_STEP + 21)
        SetChips(CHIPS_MAX);
    fprintf(stderr, "[dd] tap (%d,%d) -> %d\n", x, y, id);
}

int main(int argc, char** argv) {
    (void)argc;
    char dir[512] = ".";
    if (strrchr(argv[0], '/')) {
        snprintf(dir, sizeof dir, "%s", argv[0]);
        *strrchr(dir, '/') = 0;
    }
    if (getenv("DD_DATA_DIR"))
        snprintf(dir, sizeof dir, "%s", getenv("DD_DATA_DIR"));
    if (!RaClient_Open(getenv("DD_HOST") ? getenv("DD_HOST") : "127.0.0.1", 55355)) {
        fprintf(stderr, "[dd] cannot open the RetroArch command socket\n");
        return 1;
    }
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0 || TTF_Init() != 0) {
        fprintf(stderr, "[dd] SDL init failed: %s\n", SDL_GetError());
        return 1;
    }
    const bool windowed = getenv("DD_WINDOWED") != NULL || SDL_GetNumVideoDisplays() < 2;
    Uint32 flags = SDL_WINDOW_SHOWN;
    int px = SDL_WINDOWPOS_UNDEFINED, py = SDL_WINDOWPOS_UNDEFINED;
    if (!windowed) {
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_BORDERLESS;
        px = SDL_WINDOWPOS_UNDEFINED_DISPLAY(1);
        py = SDL_WINDOWPOS_UNDEFINED_DISPLAY(1);
    }
    /* the sway config moves windows titled "...Bottom Screen" to DSI-1 */
    sWin = SDL_CreateWindow("Drill Dozer - Bottom Screen", px, py, W, H, flags);
    if (!sWin) {
        fprintf(stderr, "[dd] window failed: %s\n", SDL_GetError());
        return 1;
    }
    sRen = SDL_CreateRenderer(sWin, -1, SDL_RENDERER_ACCELERATED);
    if (!sRen)
        sRen = SDL_CreateRenderer(sWin, -1, SDL_RENDERER_SOFTWARE);
    SDL_RenderSetLogicalSize(sRen, W, H);
    SDL_ShowCursor(SDL_DISABLE);
    char fontPath[560];
    snprintf(fontPath, sizeof fontPath, "%s/DejaVuSans.ttf", dir);
    sFont22 = TTF_OpenFont(fontPath, 22);
    sFont18 = TTF_OpenFont(fontPath, 18);
    sFont14 = TTF_OpenFont(fontPath, 14);
    sFont12 = TTF_OpenFont(fontPath, 12);
    if (!sFont22 || !sFont18 || !sFont14 || !sFont12) {
        fprintf(stderr, "[dd] cannot load font %s: %s\n", fontPath, TTF_GetError());
        return 1;
    }
    sTouchFd = OpenBottomTouch();

    /* QA: DD_TEST_TAPS="ms,x,y;ms,x,y" injects taps through the real tap handler */
    struct { unsigned ms; int x, y; bool done; } taps[32];
    int tapCount = 0;
    if (getenv("DD_TEST_TAPS")) {
        char* spec = strdup(getenv("DD_TEST_TAPS"));
        for (char* tok = strtok(spec, ";"); tok && tapCount < 32; tok = strtok(NULL, ";")) {
            unsigned ms;
            int x, y;
            if (sscanf(tok, "%u,%d,%d", &ms, &x, &y) == 3)
                taps[tapCount++] = (typeof(taps[0])){ ms, x, y, false };
        }
        free(spec);
    }

    const Uint32 startMs = SDL_GetTicks();
    Uint32 lastPoll = 0;
    unsigned silent = 0;
    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                running = false;
            else if (e.type == SDL_FINGERDOWN && sTouchFd < 0)
                OnTap((int)(e.tfinger.x * W), (int)(e.tfinger.y * H));
            else if (e.type == SDL_MOUSEBUTTONDOWN && sTouchFd < 0)
                OnTap(e.button.x, e.button.y);
        }
        PumpTouch();
        if (sPendingTap) {
            sPendingTap = false;
            OnTap(sTapX, sTapY);
        }
        for (int i = 0; i < tapCount; i++)
            if (!taps[i].done && SDL_GetTicks() - startMs >= taps[i].ms) {
                taps[i].done = true;
                fprintf(stderr, "[dd] test tap (%d,%d)\n", taps[i].x, taps[i].y);
                OnTap(taps[i].x, taps[i].y);
            }
        const Uint32 now = SDL_GetTicks();
        if (now - lastPoll >= 200) {
            lastPoll = now;
            uint8_t code[4] = { 0 };
            if (RaClient_Read(DD_ROM_CODE_ADDR, code, 4) && memcmp(code, "V49E", 4) == 0 && PollState()) {
                if (!sConnected)
                    fprintf(stderr, "[dd] connected to RetroArch\n");
                sConnected = true;
                silent = 0;
            } else if (++silent > 5u * (sConnected ? MAX_SILENT_SECONDS : STARTUP_GRACE_SECONDS)) {
                fprintf(stderr, "[dd] RetroArch is gone, exiting\n");
                break;
            }
        }
        DrawFrame();
        SDL_Delay(16);
    }
    RaClient_Close();
    if (sTouchFd >= 0)
        close(sTouchFd);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
