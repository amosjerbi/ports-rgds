/* cima-bottom: touch companion for CIMA: The Enemy running in RetroArch's mGBA core on the RG DS.
 * Runs on the bottom panel (DSI-1). Reads the party roster over RetroArch's network command interface (UDP 55355). */
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cima_data.h"
#include "ra_client.h"

#define W 640
#define H 480
#define HDR_H 58
#define TAB_Y 424
#define TAB_H 56
#define MAX_SILENT_SECONDS 20
#define STARTUP_GRACE_SECONDS 180

#include "ui_common.h"

enum { H_SCROLL_UP = 90, H_SCROLL_DN = 91, H_ROW = 200 };
#define ROW_H 44
#define LIST_Y (HDR_H + 8)
#define LIST_H (TAB_Y - LIST_Y - 6)
#define VISIBLE (LIST_H / ROW_H)

static const char* const kName[UNIT_COUNT] = { "Ark J", "Ivy F", "Vanrose U", "Shelley Y", "Jean H", "Eberle H", "Halley H",
                                               "Emmy H", "Philis S", "Telmia S", "Doug O", "Ileyda O", "Rick G", "Diana I",
                                               "Yurald D", "Claude L" };
static uint8_t sUnits[UNIT_COUNT][UNIT_STRIDE];
static int sScroll, sSel = -1;
static bool sConnected, sValid;

static bool PollState(void) {
    if (!RaClient_Read(UNIT_BASE, sUnits, sizeof sUnits))
        return false;
    sValid = sUnits[0][U_HP + 1] > 0 && sUnits[0][U_HP] <= sUnits[0][U_HP + 1];
    return true;
}

static void DrawHeader(void) {
    FillRect(0, 0, W, HDR_H, C_PANEL);
    FillRect(0, HDR_H - 2, W, 2, C_GOLD);
    Text(sFont22, 14, 14, 0, C_GOLD, "CIMA: THE ENEMY");
    Text(sFont14, 626, 20, 2, C_DIM, "ROSTER  HP  / four stats");
}

static void DrawList(void) {
    const int maxs = UNIT_COUNT > VISIBLE ? UNIT_COUNT - VISIBLE : 0;
    if (sScroll < 0) sScroll = 0;
    if (sScroll > maxs) sScroll = maxs;
    const int listW = W - 52;
    for (int v = 0; v < VISIBLE; v++) {
        const int r = sScroll + v;
        if (r >= UNIT_COUNT)
            break;
        const uint8_t* u = sUnits[r];
        const int y = LIST_Y + v * ROW_H, hp = u[U_HP], mx = u[U_HP + 1];
        FillRect(4, y, listW, ROW_H - 4, r == sSel ? C_SEL : (r & 1 ? C_PANEL : C_PANEL2));
        if (r == sSel)
            FrameRect(4, y, listW, ROW_H - 4, C_GOLD);
        Text(sFont18, 14, y + 9, 0, C_CREAM, "%s", kName[r]);
        FillRect(168, y + 15, 120, 8, C_BG);
        FillRect(168, y + 15, mx ? 120 * (hp > mx ? mx : hp) / mx : 0, 8, C_GREEN);
        Text(sFont14, 296, y + 10, 0, C_CREAM, "%d/%d", hp, mx);
        for (int k = 0; k < 4; k++)
            Text(sFont18, 400 + k * 52, y + 9, 0, C_GOLD, "%d", u[U_STAT + k]);
        AddHit(4, y, listW, ROW_H - 4, H_ROW + v);
    }
    const int bx = W - 44;
    FillRect(bx, LIST_Y, 40, LIST_H / 2 - 2, C_PANEL2);
    FrameRect(bx, LIST_Y, 40, LIST_H / 2 - 2, C_DIM);
    Text(sFont22, bx + 20, LIST_Y + LIST_H / 4 - 16, 1, C_CREAM, "^");
    FillRect(bx, LIST_Y + LIST_H / 2 + 2, 40, LIST_H / 2 - 2, C_PANEL2);
    FrameRect(bx, LIST_Y + LIST_H / 2 + 2, 40, LIST_H / 2 - 2, C_DIM);
    Text(sFont22, bx + 20, LIST_Y + LIST_H * 3 / 4 - 14, 1, C_CREAM, "v");
    AddHit(bx, LIST_Y, 40, LIST_H / 2 - 2, H_SCROLL_UP);
    AddHit(bx, LIST_Y + LIST_H / 2 + 2, 40, LIST_H / 2 - 2, H_SCROLL_DN);
}

static void DrawDetail(void) {
    FillRect(0, TAB_Y - 2, W, H - TAB_Y + 2, C_PANEL);
    FillRect(0, TAB_Y - 2, W, 2, C_GOLD);
    if (sSel < 0) {
        Text(sFont14, W / 2, TAB_Y + 20, 1, C_DIM, "Tap a character. Values are read live from the game.");
        return;
    }
    const uint8_t* u = sUnits[sSel];
    Text(sFont22, 14, TAB_Y + 6, 0, C_GOLD, "%s", kName[sSel]);
    Text(sFont18, 14, TAB_Y + 36, 0, C_CREAM, "HP %d / %d     stats %d  %d  %d  %d", u[U_HP], u[U_HP + 1], u[U_STAT],
         u[U_STAT + 1], u[U_STAT + 2], u[U_STAT + 3]);
}

static void DrawStatus(const char* l1, const char* l2) {
    FillRect(0, 0, W, H, C_BG);
    Text(sFont22, W / 2, 170, 1, C_GOLD, "CIMA: THE ENEMY");
    Text(sFont18, W / 2, 214, 1, C_CREAM, "%s", l1);
    Text(sFont14, W / 2, 246, 1, C_DIM, "%s", l2);
}

static void DrawFrame(void) {
    SDL_SetRenderDrawColor(sRen, C_BG.r, C_BG.g, C_BG.b, 255);
    SDL_RenderClear(sRen);
    sHitCount = 0;
    if (!sConnected) {
        DrawStatus("Waiting for RetroArch...", "Launch CIMA: The Enemy (mGBA core)");
    } else if (!sValid) {
        DrawStatus("Start or load a game", "The roster is not available yet");
    } else {
        DrawHeader();
        DrawList();
        DrawDetail();
    }
    SDL_RenderPresent(sRen);
}

static void OnTap(int x, int y) {
    const int id = HitTest(x, y);
    if (id < 0)
        return;
    if (id == H_SCROLL_UP) sScroll -= 3;
    else if (id == H_SCROLL_DN) sScroll += 3;
    else if (id >= H_ROW && id < H_ROW + 20) sSel = sScroll + (id - H_ROW);
    fprintf(stderr, "[cima] tap (%d,%d) -> %d\n", x, y, id);
}

static void ApplyDrag(void) {
    if (abs(sDragDy) < ROW_H / 2)
        return;
    if (sTy >= LIST_Y && sTy < LIST_Y + LIST_H)
        sScroll -= sDragDy / ROW_H ? sDragDy / ROW_H : (sDragDy > 0 ? 1 : -1);
    sDragDy = 0;
}

int main(int argc, char** argv) {
    (void)argc;
    char dir[512] = ".";
    if (strrchr(argv[0], '/')) {
        snprintf(dir, sizeof dir, "%s", argv[0]);
        *strrchr(dir, '/') = 0;
    }
    if (getenv("CIMA_DATA_DIR"))
        snprintf(dir, sizeof dir, "%s", getenv("CIMA_DATA_DIR"));
    if (!RaClient_Open(getenv("CIMA_HOST") ? getenv("CIMA_HOST") : "127.0.0.1", 55355)) {
        fprintf(stderr, "[cima] cannot open the RetroArch command socket\n");
        return 1;
    }
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0 || TTF_Init() != 0) {
        fprintf(stderr, "[cima] SDL init failed: %s\n", SDL_GetError());
        return 1;
    }
    const bool windowed = getenv("CIMA_WINDOWED") != NULL || SDL_GetNumVideoDisplays() < 2;
    Uint32 flags = SDL_WINDOW_SHOWN;
    int px = SDL_WINDOWPOS_UNDEFINED, py = SDL_WINDOWPOS_UNDEFINED;
    if (!windowed) {
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_BORDERLESS;
        px = SDL_WINDOWPOS_UNDEFINED_DISPLAY(1);
        py = SDL_WINDOWPOS_UNDEFINED_DISPLAY(1);
    }
    /* the sway config moves windows titled "...Bottom Screen" to DSI-1 */
    sWin = SDL_CreateWindow("CIMA - Bottom Screen", px, py, W, H, flags);
    if (!sWin) {
        fprintf(stderr, "[cima] window failed: %s\n", SDL_GetError());
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
        fprintf(stderr, "[cima] cannot load font %s: %s\n", fontPath, TTF_GetError());
        return 1;
    }
    sTouchFd = OpenBottomTouch();

    /* QA: CIMA_TEST_TAPS="ms,x,y;ms,x,y" injects taps through the real tap handler */
    struct { unsigned ms; int x, y; bool done; } taps[32];
    int tapCount = 0;
    if (getenv("CIMA_TEST_TAPS")) {
        char* spec = strdup(getenv("CIMA_TEST_TAPS"));
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
        ApplyDrag();
        if (sPendingTap) {
            sPendingTap = false;
            OnTap(sTapX, sTapY);
        }
        for (int i = 0; i < tapCount; i++)
            if (!taps[i].done && SDL_GetTicks() - startMs >= taps[i].ms) {
                taps[i].done = true;
                fprintf(stderr, "[cima] test tap (%d,%d)\n", taps[i].x, taps[i].y);
                OnTap(taps[i].x, taps[i].y);
            }
        const Uint32 now = SDL_GetTicks();
        if (now - lastPoll >= 200) {
            lastPoll = now;
            uint8_t code[4] = { 0 };
            if (RaClient_Read(CIMA_ROM_CODE_ADDR, code, 4) && memcmp(code, "BCME", 4) == 0 && PollState()) {
                if (!sConnected)
                    fprintf(stderr, "[cima] connected to RetroArch\n");
                sConnected = true;
                silent = 0;
            } else if (++silent > 5u * (sConnected ? MAX_SILENT_SECONDS : STARTUP_GRACE_SECONDS)) {
                fprintf(stderr, "[cima] RetroArch is gone, exiting\n");
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
