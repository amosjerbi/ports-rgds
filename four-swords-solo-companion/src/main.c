/* Zelda: Four Swords solo companion for the Anystream RG DS: runs on the bottom panel next to RetroArch's mGBA core.
 * The solo ROM patch keeps a control mask (bit n = Player n+1 follows the pad) and a player count in EWRAM 0x0203FFD0;
 * Select on the pad cycles the Links, then ALL. This app shows and sets both over RetroArch's network command interface (UDP 55355). */
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
#define RAM_CTRL 0x0203FFD0u  /* +0 control mask (bit n = Player n+1 follows the pad), +3 player count (2..4) */

#include "ui_common.h"

/* Minish Cap look: pale yellow backdrop, stone panel, blue lettering, white tabs along the bottom */
static const Col M_BG = { 248, 240, 160 }, M_STONE = { 224, 216, 216 }, M_STONE_D = { 186, 176, 178 }, M_STONE_L = { 240, 234, 234 },
                 M_SLOT = { 202, 194, 198 }, M_BLUE = { 28, 76, 196 }, M_TABEDGE = { 40, 160, 240 }, M_TAG = { 112, 34, 24 },
                 M_ORANGE = { 248, 96, 32 }, M_GREEN = { 80, 200, 70 }, M_GOLD = { 240, 200, 56 }, M_HINT = { 112, 102, 116 }, M_INK = { 40, 36, 52 };
static const Col LINK_COL[4] = { { 64, 176, 64 }, { 216, 52, 52 }, { 66, 98, 226 }, { 160, 76, 200 } };
static const char* LINK_NAME[4] = { "GREEN", "RED", "BLUE", "PURPLE" };
enum { H_SLOT = 1, H_ALL = 10, H_CNT = 20 };

static int sMask = 1, sCount = 2;
static bool sConnected;

static bool PollState(void) {
    uint8_t b[4];
    if (!RaClient_Read(RAM_CTRL, b, 4))
        return false;
    sMask = b[0] ? b[0] & 15 : 1;
    sCount = b[3] >= 2 && b[3] <= 4 ? b[3] : 2;
    return true;
}
static void WriteMask(int m) {
    const uint8_t b = (uint8_t)m;
    if (RaClient_Write(RAM_CTRL, &b, 1))
        sMask = m;
}
static void WriteCount(int n) {
    const uint8_t b = (uint8_t)n;
    if (!RaClient_Write(RAM_CTRL + 3, &b, 1))
        return;
    sCount = n;
    const int full = (1 << n) - 1;
    if (sMask & ~full)
        WriteMask(1);
}

/* a little Link, 12x12 "pixels" */
static void DrawLink(int cx, int top, int px, Col c, bool dim) {
    static const char* map[12] = { "....HHHH....", "...HHHHHH...", "..HHHHHHHH..", "..HHssssHH..", "..ssksssks..", "..ssssssss..",
                                   "...tttttt...", "..tttbbttt..", ".sttttttttts", "..tttttttt..", "..tt....tt..", "..kk....kk.." };
    for (int y = 0; y < 12; y++)
        for (int x = 0; x < 12; x++) {
            Col o;
            switch (map[y][x]) {
            case 'H': case 't': o = c; break;
            case 's': o = (Col){ 250, 214, 170 }; break;
            case 'k': o = M_INK; break;
            case 'b': o = (Col){ 150, 98, 40 }; break;
            default: continue;
            }
            if (dim) {
                o.r = (uint8_t)((o.r + M_SLOT.r * 3) / 4);
                o.g = (uint8_t)((o.g + M_SLOT.g * 3) / 4);
                o.b = (uint8_t)((o.b + M_SLOT.b * 3) / 4);
            }
            FillRect(cx - 6 * px + x * px, top + y * px, px, px, o);
        }
}

static void Brackets(int x, int y, int w, int h, Col c) {
    const int L = 16, T = 4;
    FillRect(x, y, L, T, c); FillRect(x, y, T, L, c);
    FillRect(x + w - L, y, L, T, c); FillRect(x + w - T, y, T, L, c);
    FillRect(x, y + h - T, L, T, c); FillRect(x, y + h - L, T, L, c);
    FillRect(x + w - L, y + h - T, L, T, c); FillRect(x + w - T, y + h - L, T, L, c);
}

static void Tri(int x, int y, int s, Col c) { /* little Triforce-ish corner mark */
    for (int i = 0; i < s; i++)
        FillRect(x + (s - 1 - i), y + i, 2 * i + 1, 1, c);
}

static void StonePanel(int x, int y, int w, int h) {
    FillRect(x, y, w, h, M_STONE_D);
    FillRect(x + 4, y + 4, w - 8, h - 8, M_STONE_L);
    FillRect(x + 10, y + 10, w - 20, h - 20, M_STONE);
    for (int i = 14; i < w - 14; i += 4) {
        FillRect(x + i, y + 6, 2, 2, M_STONE_D);
        FillRect(x + i, y + h - 8, 2, 2, M_STONE_D);
    }
    for (int j = 14; j < h - 14; j += 4) {
        FillRect(x + 6, y + j, 2, 2, M_STONE_D);
        FillRect(x + w - 8, y + j, 2, 2, M_STONE_D);
    }
    Tri(x + 6, y + 6, 12, M_GOLD);
    Tri(x + w - 24, y + 6, 12, M_GOLD);
    Tri(x + 6, y + h - 20, 12, M_GOLD);
    Tri(x + w - 24, y + h - 20, 12, M_GOLD);
}

static void Tab(int x, int y, int w, int h, int id, const char* label, bool active) {
    FillRect(x, y, w, h, C_WHITE);
    FrameRect(x, y, w, h, M_TABEDGE);
    FrameRect(x + 1, y + 1, w - 2, h - 2, M_TABEDGE);
    Text(sFont22, x + w / 2, y + 7, 1, M_BLUE, "%s", label);
    if (active)
        Brackets(x - 2, y - 2, w + 4, h + 4, M_GREEN);
    AddHit(x, y, w, h, id);
}

static void DrawFrame(void) {
    sHitCount = 0;
    SetCol(M_BG, 255);
    SDL_RenderClear(sRen);
    StonePanel(14, 10, 612, 396);
    /* red label tag */
    FillRect(250, 20, 140, 28, M_TAG);
    FrameRect(250, 20, 140, 28, C_WHITE);
    Text(sFont18, 320, 23, 1, C_WHITE, "PLAYERS");

    const int full = (1 << sCount) - 1;
    const bool all = sCount > 1 && (sMask & full) == full;
    for (int i = 0; i < 4; i++) {
        const int x = 44 + i * 142, y = 70, w = 126, h = 150;
        const bool on = i < sCount;
        FillRect(x, y, w, h, M_SLOT);
        FrameRect(x, y, w, h, M_STONE_D);
        DrawLink(x + w / 2, y + 22, 7, LINK_COL[i], !on);
        Text(sFont18, x + w / 2, y + 112, 1, on ? M_BLUE : M_STONE_D, "PLAYER %d", i + 1);
        Text(sFont12, x + w / 2, y + 133, 1, on ? M_HINT : M_STONE_D, "%s", on ? LINK_NAME[i] : "(off)");
        if (on && (sMask >> i & 1))
            Brackets(x - 3, y - 3, w + 6, h + 6, M_ORANGE);
        if (on)
            AddHit(x, y, w, h, H_SLOT + i);
    }
    /* ALL button, styled like the equipped-item discs */
    const int bx = 44, by = 246, bw = 554, bh = 50;
    FillRect(bx, by, bw, bh, C_WHITE);
    FrameRect(bx, by, bw, bh, M_TABEDGE);
    FrameRect(bx + 1, by + 1, bw - 2, bh - 2, M_TABEDGE);
    Text(sFont22, bx + bw / 2, by + 5, 1, M_BLUE, "ALL LINKS MOVE TOGETHER");
    Text(sFont12, bx + bw / 2, by + 32, 1, M_HINT, all ? "ON - every Link copies your moves" : "OFF - tap to make every Link copy your moves");
    if (all)
        Brackets(bx - 3, by - 3, bw + 6, bh + 6, M_GREEN);
    AddHit(bx, by, bw, bh, H_ALL);

    if (sConnected) {
        if (all)
            Text(sFont14, 320, 316, 1, M_BLUE, "Controlling: ALL %d Links", sCount);
        else {
            char who[64] = "";
            for (int i = 0; i < sCount; i++)
                if (sMask >> i & 1)
                    snprintf(who + strlen(who), sizeof who - strlen(who), "%sPlayer %d", who[0] ? " + " : "", i + 1);
            Text(sFont14, 320, 316, 1, M_BLUE, "Controlling: %s", who);
        }
        Text(sFont12, 320, 342, 1, M_HINT, "Tap a Link to control only that one. SELECT on the pad cycles Links, then ALL.");
        Text(sFont12, 320, 362, 1, M_HINT, "Choose the number of players below before you start a stage.");
    } else {
        Text(sFont14, 320, 330, 1, M_TAG, "Waiting for the game...");
    }

    for (int k = 0; k < 3; k++) {
        char lab[24];
        snprintf(lab, sizeof lab, "%d PLAYERS", k + 2);
        Tab(6 + k * 196, 420, 188, 40, H_CNT + k + 2, lab, sCount == k + 2);
    }
    SDL_RenderPresent(sRen);
}

static void OnTap(int x, int y) {
    const int id = HitTest(x, y);
    if (id >= H_SLOT && id < H_SLOT + 4)
        WriteMask(1 << (id - H_SLOT));
    else if (id == H_ALL)
        WriteMask((1 << sCount) - 1);
    else if (id >= H_CNT + 2 && id <= H_CNT + 4)
        WriteCount(id - H_CNT);
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
