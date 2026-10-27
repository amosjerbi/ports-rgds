/* Zelda: Four Swords solo companion for the Anystream RG DS: runs on the bottom panel next to RetroArch's mGBA core.
 * The solo ROM patch keeps a "who do the controls drive" byte in EWRAM (0x0203FFF0: 0 = Player 1, 1 = Player 2);
 * Select on the pad flips it too. This app shows and sets it over RetroArch's network command interface (UDP 55355). */
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ra_client.h"

#define W 640
#define H 480
#define MAX_SILENT_SECONDS 20
#define STARTUP_GRACE_SECONDS 180
#define RAM_SWITCH 0x0203FFF0u

#include "ui_common.h"

static const Col B_BG = { 8, 22, 62 }, B_PANEL = { 14, 44, 112 }, B_EDGE = { 120, 170, 255 }, B_DARK = { 6, 16, 44 },
                 L_GREEN = { 70, 190, 90 }, L_RED = { 214, 52, 56 };
enum { H_P1 = 1, H_P2 };
static int sWho = 0; /* 0 = Player 1, 1 = Player 2 */
static bool sConnected;

static bool PollState(void) {
    uint8_t b;
    if (!RaClient_Read(RAM_SWITCH, &b, 1))
        return false;
    sWho = b & 1;
    return true;
}
static void SetWho(int who) {
    const uint8_t b = (uint8_t)who;
    if (RaClient_Write(RAM_SWITCH, &b, 1))
        sWho = who;
}

static void Button(int x, int y, int w, int h, int id, const char* name, const char* sub, Col c, bool active) {
    FillRect(x, y, w, h, active ? c : B_PANEL);
    FrameRect(x, y, w, h, active ? C_WHITE : B_EDGE);
    FrameRect(x + 1, y + 1, w - 2, h - 2, active ? C_WHITE : B_EDGE);
    /* a little Link: tunic-coloured block with a face */
    const int cx = x + w / 2, cy = y + 70;
    FillRect(cx - 24, cy - 8, 48, 46, active ? C_WHITE : c);
    FillRect(cx - 18, cy - 40, 36, 32, (Col){ 250, 214, 170 });
    FillRect(cx - 24, cy - 48, 48, 14, active ? B_DARK : c);
    FillRect(cx - 10, cy - 28, 5, 8, B_DARK);
    FillRect(cx + 6, cy - 28, 5, 8, B_DARK);
    Text(sFont22, cx, y + h - 70, 1, active ? C_WHITE : C_CREAM, "%s", name);
    Text(sFont14, cx, y + h - 38, 1, active ? C_WHITE : C_DIM, "%s", sub);
    AddHit(x, y, w, h, id);
}

static void DrawFrame(void) {
    sHitCount = 0;
    SetCol(B_BG, 255);
    SDL_RenderClear(sRen);
    FillRect(0, 0, W, 56, B_PANEL);
    FrameRect(0, 0, W, 56, B_EDGE);
    Text(sFont22, W / 2, 14, 1, C_CREAM, "FOUR SWORDS  -  WHO DO I CONTROL?");
    Button(24, 80, 280, 260, H_P1, "PLAYER 1", "green Link", L_GREEN, sWho == 0);
    Button(336, 80, 280, 260, H_P2, "PLAYER 2", "red Link", L_RED, sWho == 1);
    FillRect(24, 366, W - 48, 74, B_PANEL);
    FrameRect(24, 366, W - 48, 74, B_EDGE);
    if (sConnected) {
        Text(sFont18, W / 2, 380, 1, C_CREAM, "Controlling %s. The other Link stands still.", sWho ? "Player 2" : "Player 1");
        Text(sFont14, W / 2, 412, 1, C_DIM, "Tap a Link, or press SELECT on the pad, to switch.");
    } else {
        Text(sFont18, W / 2, 380, 1, C_YELLOW, "Waiting for the game...");
        Text(sFont14, W / 2, 412, 1, C_DIM, "Start Four Swords from the game menu.");
    }
    SDL_RenderPresent(sRen);
}

static void OnTap(int x, int y) {
    const int id = HitTest(x, y);
    if (id == H_P1)
        SetWho(0);
    else if (id == H_P2)
        SetWho(1);
    fprintf(stderr, "[fs] tap (%d,%d) -> %d\n", x, y, id);
}

int main(int argc, char** argv) {
    (void)argc;
    char dir[512] = ".";
    if (strrchr(argv[0], '/')) {
        snprintf(dir, sizeof dir, "%s", argv[0]);
        *strrchr(dir, '/') = 0;
    }
    if (getenv("FS_DATA_DIR"))
        snprintf(dir, sizeof dir, "%s", getenv("FS_DATA_DIR"));
    if (!RaClient_Open("127.0.0.1", 55355)) {
        fprintf(stderr, "[fs] cannot open the RetroArch command socket\n");
        return 1;
    }
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0 || TTF_Init() != 0) {
        fprintf(stderr, "[fs] SDL init failed: %s\n", SDL_GetError());
        return 1;
    }
    const bool windowed = getenv("FS_WINDOWED") != NULL || SDL_GetNumVideoDisplays() < 2;
    Uint32 flags = SDL_WINDOW_SHOWN;
    int px = SDL_WINDOWPOS_UNDEFINED, py = SDL_WINDOWPOS_UNDEFINED;
    if (!windowed) {
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_BORDERLESS;
        px = SDL_WINDOWPOS_UNDEFINED_DISPLAY(1);
        py = SDL_WINDOWPOS_UNDEFINED_DISPLAY(1);
    }
    /* the sway config moves windows titled "...Bottom Screen" to DSI-1 */
    sWin = SDL_CreateWindow("Four Swords - Bottom Screen", px, py, W, H, flags);
    if (!sWin) {
        fprintf(stderr, "[fs] window failed: %s\n", SDL_GetError());
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
        fprintf(stderr, "[fs] cannot load font %s: %s\n", fontPath, TTF_GetError());
        return 1;
    }
    sTouchFd = OpenBottomTouch();

    /* QA: FS_TEST_TAPS="ms,x,y;ms,x,y" injects taps through the real tap handler */
    struct { unsigned ms; int x, y; bool done; } taps[32];
    int tapCount = 0;
    if (getenv("FS_TEST_TAPS")) {
        char* spec = strdup(getenv("FS_TEST_TAPS"));
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
        sDragDy = 0;
        if (sPendingTap) {
            sPendingTap = false;
            OnTap(sTapX, sTapY);
        }
        for (int i = 0; i < tapCount; i++)
            if (!taps[i].done && SDL_GetTicks() - startMs >= taps[i].ms) {
                taps[i].done = true;
                OnTap(taps[i].x, taps[i].y);
            }
        const Uint32 now = SDL_GetTicks();
        if (now - lastPoll >= 150) {
            lastPoll = now;
            if (PollState()) {
                if (!sConnected)
                    fprintf(stderr, "[fs] connected to RetroArch\n");
                sConnected = true;
                silent = 0;
            } else if (++silent > 7u * (sConnected ? MAX_SILENT_SECONDS : STARTUP_GRACE_SECONDS)) {
                fprintf(stderr, "[fs] RetroArch is gone, exiting\n");
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
