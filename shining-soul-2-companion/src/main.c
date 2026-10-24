/* ss2-bottom: touch companion for Shining Soul II running in RetroArch's mGBA core on the RG DS.
 * Runs on the bottom panel (DSI-1). Reads the hero's status over RetroArch's network command interface and presses the
 * game's R / L buttons through RetroArch's network gamepad. */
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "ss2_data.h"
#include "ra_client.h"

#define W 640
#define H 480
#define HDR_H 58
#define TAB_Y 424
#define TAB_H 56
#define MAX_SILENT_SECONDS 20
#define STARTUP_GRACE_SECONDS 180

#include "ui_common.h"

static int sHp, sMaxHp, sSp, sMaxSp, sGold;
static int sStr, sDex, sInt, sVit, sAtk, sDef;
static char sName[NAME_LEN + 1];
static bool sConnected, sValid;

static uint16_t U16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t U32(const uint8_t* p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

static bool PollState(void) {
    uint8_t v[20], hp[4], nm[NAME_LEN * 2], st[8], g[2], ad[4];
    if (!RaClient_Read(ADDR_VITALS, v, sizeof v) || !RaClient_Read(ADDR_HP_LIVE, hp, 4) || !RaClient_Read(ADDR_NAME, nm, sizeof nm) ||
        !RaClient_Read(ADDR_STATS, st, sizeof st) || !RaClient_Read(ADDR_GOLD, g, 2) || !RaClient_Read(ADDR_ATKDEF, ad, 4))
        return false;
    sHp = (int)U32(hp);
    sMaxHp = (int)U32(v + 12);
    sSp = (int)U32(v + 4) / 10;
    sMaxSp = (int)U32(v + 16) / 10;
    sStr = U16(st);
    sDex = U16(st + 2);
    sInt = U16(st + 4);
    sVit = U16(st + 6);
    sGold = U16(g);
    sAtk = U16(ad);
    sDef = U16(ad + 2);
    for (int i = 0; i < NAME_LEN; i++) {
        const int c = U16(nm + i * 2);
        const int ch = c >= 0x21 && c <= 0x3A ? 'A' + (c - 0x21) : c >= 0x41 && c <= 0x5A ? 'a' + (c - 0x41) : ' ';
        sName[i] = i == 0 || ch == ' ' ? (char)ch : (char)(ch >= 'A' && ch <= 'Z' ? ch + 32 : ch);
    }
    sName[NAME_LEN] = 0;
    sValid = sMaxHp > 0 && sMaxHp < 5000 && sHp >= 0 && sHp <= sMaxHp;
    return true;
}

static void Bar(int x, int y, int w, int cur, int max, Col c) {
    FillRect(x - 1, y - 1, w + 2, 12, C_TRIM);
    FillRect(x, y, w, 10, C_FIELD);
    if (max > 0) FillRect(x, y, w * (cur > max ? max : cur) / max, 10, c);
}

enum { H_USE = 100, H_SWAP };

static void Panel(int x, int y, int w, int h) {
    FillRect(x, y, w, h, C_PANEL);
    FrameRect(x, y, w, h, C_TRIM);
    FrameRect(x + 1, y + 1, w - 2, h - 2, C_PANEL2);
}

static void DrawStatus(void) {
    const int pw = W - 16;
    Panel(8, HDR_H + 8, pw, 126);
    Text(sFont14, 24, HDR_H + 20, 0, C_CREAM, "HP");
    Text(sFont18, W - 24, HDR_H + 16, 2, C_CREAM, "%d / %d", sHp, sMaxHp);
    Bar(24, HDR_H + 44, pw - 32, sHp, sMaxHp, C_GREEN);
    Text(sFont14, 24, HDR_H + 74, 0, C_CREAM, "SP");
    Text(sFont18, W - 24, HDR_H + 70, 2, C_CREAM, "%d / %d", sSp, sMaxSp);
    Bar(24, HDR_H + 98, pw - 32, sSp, sMaxSp, C_ORANGE);

    const int y0 = HDR_H + 144;
    Panel(8, y0, pw, 112);
    Text(sFont14, 24, y0 + 8, 0, C_GOLD, "STATS");
    static const char* const kN[6] = { "ATK", "DEF", "VIT", "STR", "DEX", "INT" };
    const int v[6] = { sAtk, sDef, sVit, sStr, sDex, sInt };
    for (int i = 0; i < 6; i++) {
        const int x = 24 + (i / 3) * (pw / 2), y = y0 + 32 + (i % 3) * 26;
        Text(sFont18, x, y, 0, C_CREAM, "%s", kN[i]);
        Text(sFont18, x + pw / 2 - 48, y, 2, C_GOLD, "%d", v[i]);
    }

    const int by = y0 + 124, bw = (pw - 12) / 2;
    FillRect(8, by, bw, 64, C_PANEL2);
    FrameRect(8, by, bw, 64, C_GOLD);
    Text(sFont22, 8 + bw / 2, by + 8, 1, C_GOLD, "USE ITEM");
    Text(sFont12, 8 + bw / 2, by + 40, 1, C_CREAM, "presses R (the ITEM slot)");
    AddHit(8, by, bw, 64, H_USE);
    FillRect(20 + bw, by, bw, 64, C_PANEL2);
    FrameRect(20 + bw, by, bw, 64, C_TRIM);
    Text(sFont22, 20 + bw + bw / 2, by + 8, 1, C_CREAM, "SWAP WEAPON");
    Text(sFont12, 20 + bw + bw / 2, by + 40, 1, C_CREAM, "presses L (the ARM slot)");
    AddHit(20 + bw, by, bw, 64, H_SWAP);
}

static void DrawFrame(void) {
    FillRect(0, 0, W, H, C_BG);
    sHitCount = 0;
    if (!sConnected) {
        Text(sFont22, W / 2, 170, 1, C_GOLD, "SHINING SOUL II");
        Text(sFont18, W / 2, 214, 1, C_CREAM, "Waiting for RetroArch...");
    } else if (!sValid) {
        Text(sFont22, W / 2, 170, 1, C_GOLD, "SHINING SOUL II");
        Text(sFont18, W / 2, 214, 1, C_CREAM, "Load a saved game");
    } else {
        FillRect(0, 0, W, HDR_H, C_PANEL);
        FillRect(0, HDR_H - 3, W, 3, C_TRIM);
        Text(sFont22, 14, 14, 0, C_GOLD, "SHINING SOUL II");
        Text(sFont18, 340, 17, 0, C_CREAM, "%s", sName);
        Text(sFont18, 626, 17, 2, C_GOLD, "%d G", sGold);
        DrawStatus();
    }
    SDL_RenderPresent(sRen);
}

/* RetroArch's network gamepad (network_remote, UDP 55400): the companion drives the game's own item menu. */
static int sRemoteFd = -1;
static void Pad(int retropadId, int wait) {
    if (sRemoteFd < 0) sRemoteFd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sRemoteFd < 0) return;
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons(55400);
    inet_pton(AF_INET, "192.168.0.105", &a.sin_addr);
    for (int st = 1; st >= 0; st--) {
        uint32_t pkt[5] = { 0, 1, 0, (uint32_t)retropadId, (uint32_t)st };
        const ssize_t r = sendto(sRemoteFd, pkt, sizeof pkt, 0, (struct sockaddr*)&a, sizeof a);
        (void)r;
        SDL_Delay(110);
    }
    SDL_Delay(wait);
    SDL_PumpEvents();
}
enum { PAD_B = 0, PAD_L = 10, PAD_R = 11 };

static void OnTap(int x, int y) {
    const int id = HitTest(x, y);
    if (id == H_USE) Pad(PAD_R, 100);
    else if (id == H_SWAP) Pad(PAD_L, 100);
}

int main(int argc, char** argv) {
    (void)argc;
    char dir[512] = ".";
    if (strrchr(argv[0], '/')) {
        snprintf(dir, sizeof dir, "%s", argv[0]);
        *strrchr(dir, '/') = 0;
    }
    if (getenv("SS2_DATA_DIR"))
        snprintf(dir, sizeof dir, "%s", getenv("SS2_DATA_DIR"));
    if (!RaClient_Open(getenv("SS2_HOST") ? getenv("SS2_HOST") : "127.0.0.1", 55355)) {
        fprintf(stderr, "[ss2] cannot open the RetroArch command socket\n");
        return 1;
    }
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0 || TTF_Init() != 0) {
        fprintf(stderr, "[ss2] SDL init failed: %s\n", SDL_GetError());
        return 1;
    }
    const bool windowed = getenv("SS2_WINDOWED") != NULL || SDL_GetNumVideoDisplays() < 2;
    Uint32 flags = SDL_WINDOW_SHOWN;
    int px = SDL_WINDOWPOS_UNDEFINED, py = SDL_WINDOWPOS_UNDEFINED;
    if (!windowed) {
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_BORDERLESS;
        px = SDL_WINDOWPOS_UNDEFINED_DISPLAY(1);
        py = SDL_WINDOWPOS_UNDEFINED_DISPLAY(1);
    }
    /* the sway config moves windows titled "...Bottom Screen" to DSI-1 */
    sWin = SDL_CreateWindow("Shining Soul II - Bottom Screen", px, py, W, H, flags);
    if (!sWin) {
        fprintf(stderr, "[ss2] window failed: %s\n", SDL_GetError());
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
        fprintf(stderr, "[ss2] cannot load font %s: %s\n", fontPath, TTF_GetError());
        return 1;
    }
    sTouchFd = OpenBottomTouch();

    /* QA: SS2_TEST_TAPS="ms,x,y;ms,x,y" injects taps through the real tap handler */
    struct { unsigned ms; int x, y; bool done; } taps[32];
    int tapCount = 0;
    if (getenv("SS2_TEST_TAPS")) {
        char* spec = strdup(getenv("SS2_TEST_TAPS"));
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
                fprintf(stderr, "[ss2] test tap (%d,%d)\n", taps[i].x, taps[i].y);
                OnTap(taps[i].x, taps[i].y);
            }
        const Uint32 now = SDL_GetTicks();
        if (now - lastPoll >= 200) {
            lastPoll = now;
            uint8_t code[4] = { 0 };
            if (RaClient_Read(SS2_ROM_CODE_ADDR, code, 4) && memcmp(code, "AU2E", 4) == 0 && PollState()) {
                if (!sConnected)
                    fprintf(stderr, "[ss2] connected to RetroArch\n");
                sConnected = true;
                silent = 0;
            } else if (++silent > 5u * (sConnected ? MAX_SILENT_SECONDS : STARTUP_GRACE_SECONDS)) {
                fprintf(stderr, "[ss2] RetroArch is gone, exiting\n");
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
