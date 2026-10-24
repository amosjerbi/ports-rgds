/* Golden Sun (USA/Europe) companion for the Anystream RG DS: runs on the bottom panel next to RetroArch's mGBA core on the top panel.
 * Reads and writes the game's RAM over RetroArch's network command interface (UDP 55355).
 * Tabs: PARTY (stats of the four adventurers, tap HP / PP to refill), DJINN (all 28 Djinn and who holds them), TOOLS (coins, heal). */
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "gs_data.h"
#include "ra_client.h"

#define W 640
#define H 480
#define HDR_H 58
#define TAB_Y 424
#define TAB_H 56
#define BODY_Y (HDR_H + 4)
#define MAX_SILENT_SECONDS 20
#define STARTUP_GRACE_SECONDS 180

#include "ui_common.h"

/* ------------------------------------------------------------------------------------------------------------------ */
/* game state                                                                                                         */
/* ------------------------------------------------------------------------------------------------------------------ */
typedef struct {
    char name[16];
    int level, maxHp, maxPp, hp, pp, attack, defense, agility, luck;
    int power[ELEMENT_COUNT], resist[ELEMENT_COUNT];
    uint32_t djinnOwned[ELEMENT_COUNT], djinnSet[ELEMENT_COUNT];
    int setCount[ELEMENT_COUNT], ownedCount[ELEMENT_COUNT];
    uint32_t exp;
} Char;
typedef struct {
    bool valid;
    Char ch[PARTY_COUNT];
    uint32_t coins;
} State;
static State sSt;
static bool sConnected, sRomOk;

static int U16(const uint8_t* b, int o) { return b[o] | (b[o + 1] << 8); }
static uint32_t U32(const uint8_t* b, int o) { return (uint32_t)(b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | ((uint32_t)b[o + 3] << 24)); }

static bool PollState(void) {
    static uint8_t blk[PARTY_COUNT * PARTY_STRIDE];
    uint8_t coins[4];
    if (!RaClient_Read(RAM_PARTY, blk, sizeof blk))
        return false;
    if (!sRomOk) {
        uint8_t code[4] = { 0 };
        if (RaClient_Read(GS_ROM_CODE_ADDR, code, 4) && memcmp(code, "AGSE", 4) == 0) {
            sRomOk = true;
            fprintf(stderr, "[gs] Golden Sun (USA/Europe) detected\n");
        } else if (code[0] && memcmp(code, "AGSE", 4) != 0) {
            fprintf(stderr, "[gs] not Golden Sun (USA/Europe): %.4s\n", code);
        }
    }
    bool ok = sRomOk;
    for (int i = 0; i < PARTY_COUNT; i++) {
        const uint8_t* b = blk + i * PARTY_STRIDE;
        Char* c = &sSt.ch[i];
        memcpy(c->name, b + CH_NAME, 15);
        c->name[15] = 0;
        c->level = b[CH_LEVEL];
        c->maxHp = U16(b, CH_MAX_HP);
        c->maxPp = U16(b, CH_MAX_PP);
        c->hp = U16(b, CH_HP);
        c->pp = U16(b, CH_PP);
        c->attack = U16(b, CH_ATTACK);
        c->defense = U16(b, CH_DEFENSE);
        c->agility = U16(b, CH_AGILITY);
        c->luck = b[CH_LUCK];
        for (int e = 0; e < ELEMENT_COUNT; e++) {
            c->power[e] = U16(b, CH_ELEMENTS + 4 * e);
            c->resist[e] = U16(b, CH_ELEMENTS + 4 * e + 2);
            c->djinnOwned[e] = U32(b, CH_DJINN_OWNED + 4 * e);
            c->djinnSet[e] = U32(b, CH_DJINN_SET + 4 * e);
            c->setCount[e] = b[CH_DJINN_SET_COUNT + e];
            c->ownedCount[e] = b[CH_DJINN_OWNED_COUNT + e];
        }
        c->exp = U32(b, CH_EXP);
        if (i < 2 && !(c->name[0] && c->level >= 1 && c->level <= 99 && c->maxHp > 0 && c->maxHp < 3000 && c->hp <= c->maxHp))
            ok = false;
    }
    sSt.valid = ok;
    if (RaClient_Read(RAM_COINS, coins, 4))
        sSt.coins = U32(coins, 0);
    return true;
}

static void WriteU16(uint32_t addr, int v) {
    const uint8_t b[2] = { (uint8_t)(v & 255), (uint8_t)(v >> 8) };
    RaClient_Write(addr, b, 2);
}
static void WriteU32(uint32_t addr, uint32_t v) {
    const uint8_t b[4] = { (uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24) };
    RaClient_Write(addr, b, 4);
}
static uint32_t CharAddr(int i) { return RAM_PARTY + (uint32_t)i * PARTY_STRIDE; }

/* ------------------------------------------------------------------------------------------------------------------ */
/* UI                                                                                                                 */
/* ------------------------------------------------------------------------------------------------------------------ */
enum { TAB_PARTY, TAB_DJINN, TAB_TOOLS, TAB_COUNT };
static const char* kTabName[TAB_COUNT] = { "PARTY", "DJINN", "TOOLS" };
enum {
    H_TAB = 100,
    H_CHAR = 110, /* + member */
    H_HP = 120,
    H_PP = 121,
    H_HEAL_ALL = 130,
    H_COINS_10K = 131,
    H_COINS_100K = 132,
    H_COINS_MAX = 133
};
static int sTab = TAB_PARTY, sMember;
static char sToast[96];
static Uint32 sToastUntil;

static void Toast(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(sToast, sizeof sToast, fmt, ap);
    va_end(ap);
    sToastUntil = SDL_GetTicks() + 2200;
}

static const Col kElementCol[ELEMENT_COUNT] = { { 224, 150, 50 }, { 70, 140, 230 }, { 214, 70, 60 }, { 160, 110, 220 } };
static const Col C_GREY = { 60, 54, 72 };

static void DrawBar(int x, int y, int w, int h, int cur, int max, Col c) {
    FillRect(x, y, w, h, C_PANEL2);
    if (max > 0)
        FillRect(x, y, (int)((long)w * (cur > max ? max : cur) / max), h, c);
    FrameRect(x, y, w, h, C_DIM);
}

static void DrawHeader(void) {
    FillRect(0, 0, W, HDR_H, C_PANEL);
    FillRect(0, HDR_H - 2, W, 2, C_GOLD);
    Text(sFont22, 14, 14, 0, C_GOLD, "GOLDEN SUN");
    Text(sFont18, W - 14, 6, 2, C_YELLOW, "%u", sSt.coins);
    Text(sFont12, W - 14, 34, 2, C_DIM, "coins");
    int x = 190;
    for (int i = 0; i < PARTY_COUNT; i++) {
        const Char* c = &sSt.ch[i];
        Text(sFont12, x, 8, 0, C_CREAM, "%s", c->name);
        DrawBar(x, 26, 54, 6, c->hp, c->maxHp, c->hp * 4 < c->maxHp ? C_RED : C_GREEN);
        DrawBar(x, 36, 54, 6, c->pp, c->maxPp, C_BLUE);
        x += 66;
    }
}

static void DrawTabs(void) {
    FillRect(0, TAB_Y - 2, W, H - TAB_Y + 2, C_PANEL);
    FillRect(0, TAB_Y - 2, W, 2, C_GOLD);
    const int tw = W / TAB_COUNT;
    for (int t = 0; t < TAB_COUNT; t++) {
        const bool act = sTab == t;
        FillRect(t * tw + 3, TAB_Y + 3, tw - 6, TAB_H - 6, act ? C_RED : C_PANEL2);
        FrameRect(t * tw + 3, TAB_Y + 3, tw - 6, TAB_H - 6, act ? C_GOLD : C_DIM);
        Text(sFont18, t * tw + tw / 2, TAB_Y + 17, 1, act ? C_WHITE : C_CREAM, "%s", kTabName[t]);
        AddHit(t * tw, TAB_Y, tw, TAB_H, H_TAB + t);
    }
}

static void DrawParty(void) {
    const int chipW = 154;
    for (int i = 0; i < PARTY_COUNT; i++) {
        const int x = 4 + i * (chipW + 4), y = BODY_Y + 2;
        const bool sel = i == sMember;
        FillRect(x, y, chipW, 34, sel ? C_RED : C_PANEL2);
        FrameRect(x, y, chipW, 34, sel ? C_GOLD : C_DIM);
        Text(sFont18, x + chipW / 2, y + 6, 1, sel ? C_WHITE : C_CREAM, "%s", sSt.ch[i].name);
        AddHit(x, y, chipW, 34, H_CHAR + i);
    }
    const Char* c = &sSt.ch[sMember];
    int y = BODY_Y + 44;
    FillRect(4, y, W - 8, 110, C_PANEL);
    FrameRect(4, y, W - 8, 110, C_DIM);
    Text(sFont22, 18, y + 6, 0, C_GOLD, "%s", c->name);
    Text(sFont18, 170, y + 9, 0, C_CREAM, "Lv %d", c->level);
    Text(sFont14, W - 18, y + 10, 2, C_DIM, "Exp %u", c->exp);
    Text(sFont14, 18, y + 44, 0, C_RED, "HP");
    DrawBar(52, y + 45, 330, 16, c->hp, c->maxHp, c->hp * 4 < c->maxHp ? C_RED : C_GREEN);
    Text(sFont14, W - 18, y + 44, 2, c->hp >= c->maxHp ? C_GREEN : C_CREAM, "%d / %d", c->hp, c->maxHp);
    AddHit(4, y + 36, W - 8, 32, H_HP);
    Text(sFont14, 18, y + 76, 0, C_BLUE, "PP");
    DrawBar(52, y + 77, 330, 16, c->pp, c->maxPp, C_BLUE);
    Text(sFont14, W - 18, y + 76, 2, c->pp >= c->maxPp ? C_GREEN : C_CREAM, "%d / %d", c->pp, c->maxPp);
    AddHit(4, y + 68, W - 8, 32, H_PP);
    /* stats */
    y += 116;
    FillRect(4, y, W - 8, 40, C_PANEL);
    FrameRect(4, y, W - 8, 40, C_DIM);
    static const char* kStat[4] = { "Attack", "Defense", "Agility", "Luck" };
    const int val[4] = { c->attack, c->defense, c->agility, c->luck };
    for (int i = 0; i < 4; i++) {
        const int x = 18 + i * 154;
        Text(sFont12, x, y + 4, 0, C_DIM, "%s", kStat[i]);
        Text(sFont18, x, y + 16, 0, C_CREAM, "%d", val[i]);
    }
    /* elements */
    y += 46;
    FillRect(4, y, W - 8, 134, C_PANEL);
    FrameRect(4, y, W - 8, 134, C_DIM);
    for (int e = 0; e < ELEMENT_COUNT; e++) {
        const int x = 150 + e * 118;
        FillRect(x, y + 6, 108, 22, kElementCol[e]);
        Text(sFont12, x + 54, y + 10, 1, C_WHITE, "%s", kElementName[e]);
        Text(sFont18, x + 54, y + 38, 1, C_CREAM, "%d / %d", c->setCount[e], c->ownedCount[e]);
        Text(sFont18, x + 54, y + 70, 1, C_CREAM, "%d", c->power[e]);
        Text(sFont18, x + 54, y + 100, 1, C_CREAM, "%d", c->resist[e]);
    }
    Text(sFont14, 18, y + 41, 0, C_DIM, "Djinn set");
    Text(sFont14, 18, y + 73, 0, C_DIM, "Power");
    Text(sFont14, 18, y + 103, 0, C_DIM, "Resist");
}

static void DrawDjinn(void) {
    const int colW = 156;
    for (int e = 0; e < ELEMENT_COUNT; e++) {
        const int x = 4 + e * colW;
        FillRect(x, BODY_Y + 2, colW - 4, 24, kElementCol[e]);
        Text(sFont14, x + (colW - 4) / 2, BODY_Y + 5, 1, C_WHITE, "%s", kElementName[e]);
        for (int d = 0; d < DJINN_PER_ELEMENT; d++) {
            const int y = BODY_Y + 30 + d * 46;
            int owner = -1;
            bool set = false;
            for (int i = 0; i < PARTY_COUNT; i++)
                if ((sSt.ch[i].djinnOwned[e] >> d) & 1) {
                    owner = i;
                    set = (sSt.ch[i].djinnSet[e] >> d) & 1;
                    break;
                }
            FillRect(x, y, colW - 4, 42, owner >= 0 ? C_PANEL2 : C_PANEL);
            FrameRect(x, y, colW - 4, 42, owner >= 0 ? (set ? kElementCol[e] : C_DIM) : C_GREY);
            Text(sFont14, x + 8, y + 4, 0, owner >= 0 ? C_CREAM : C_GREY, "%s", kDjinnName[e][d]);
            if (owner >= 0) {
                Text(sFont12, x + 8, y + 22, 0, C_DIM, "%s", sSt.ch[owner].name);
                Text(sFont12, x + colW - 14, y + 22, 2, set ? C_GREEN : C_YELLOW, set ? "SET" : "STANDBY");
            } else {
                Text(sFont12, x + 8, y + 22, 0, C_GREY, "not found");
            }
        }
    }
}

static void DrawTools(void) {
    int y = BODY_Y + 6;
    FillRect(4, y, W - 8, 70, C_PANEL);
    FrameRect(4, y, W - 8, 70, C_DIM);
    Text(sFont18, 18, y + 10, 0, C_CREAM, "COINS");
    Text(sFont22, W - 18, y + 8, 2, C_YELLOW, "%u", sSt.coins);
    Text(sFont12, 18, y + 44, 0, C_DIM, "The game caps coins at 999999.");
    y += 82;
    const struct { int hit; const char* label; } btn[3] = { { H_COINS_10K, "+10,000" }, { H_COINS_100K, "+100,000" }, { H_COINS_MAX, "MAX" } };
    for (int i = 0; i < 3; i++) {
        const int x = 4 + i * 212;
        FillRect(x, y, 208, 48, C_PANEL2);
        FrameRect(x, y, 208, 48, C_GOLD);
        Text(sFont18, x + 104, y + 13, 1, C_CREAM, "%s", btn[i].label);
        AddHit(x, y, 208, 48, btn[i].hit);
    }
    y += 64;
    FillRect(4, y, W - 8, 56, C_RED);
    FrameRect(4, y, W - 8, 56, C_GOLD);
    Text(sFont18, W / 2, y + 16, 1, C_WHITE, "HEAL PARTY  (HP and PP to full)");
    AddHit(4, y, W - 8, 56, H_HEAL_ALL);
}

static void DrawNote(const char* l1, const char* l2) {
    Text(sFont22, W / 2, 180, 1, C_GOLD, "GOLDEN SUN");
    Text(sFont18, W / 2, 214, 1, C_CREAM, "%s", l1);
    Text(sFont14, W / 2, 246, 1, C_DIM, "%s", l2);
}

static void DrawFrame(void) {
    SDL_SetRenderDrawColor(sRen, C_BG.r, C_BG.g, C_BG.b, 255);
    SDL_RenderClear(sRen);
    sHitCount = 0;
    if (!sConnected) {
        DrawNote("Waiting for RetroArch...", "Launch Golden Sun (mGBA core)");
    } else if (!sSt.valid) {
        DrawNote("Load a saved game", "Choose Continue on the title screen to begin");
    } else {
        DrawHeader();
        switch (sTab) {
            case TAB_PARTY: DrawParty(); break;
            case TAB_DJINN: DrawDjinn(); break;
            case TAB_TOOLS: DrawTools(); break;
        }
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
    if (id >= H_TAB && id < H_TAB + TAB_COUNT) {
        sTab = id - H_TAB;
    } else if (id >= H_CHAR && id < H_CHAR + PARTY_COUNT) {
        sMember = id - H_CHAR;
    } else if (id == H_HP) {
        WriteU16(CharAddr(sMember) + CH_HP, sSt.ch[sMember].maxHp);
        Toast("%s: HP full", sSt.ch[sMember].name);
    } else if (id == H_PP) {
        WriteU16(CharAddr(sMember) + CH_PP, sSt.ch[sMember].maxPp);
        Toast("%s: PP full", sSt.ch[sMember].name);
    } else if (id == H_HEAL_ALL) {
        for (int i = 0; i < PARTY_COUNT; i++) {
            WriteU16(CharAddr(i) + CH_HP, sSt.ch[i].maxHp);
            WriteU16(CharAddr(i) + CH_PP, sSt.ch[i].maxPp);
        }
        Toast("Party healed");
    } else if (id >= H_COINS_10K && id <= H_COINS_MAX) {
        uint32_t v = id == H_COINS_MAX ? 999999u : sSt.coins + (id == H_COINS_10K ? 10000u : 100000u);
        if (v > 999999u)
            v = 999999u;
        WriteU32(RAM_COINS, v);
    }
    fprintf(stderr, "[gs] tap (%d,%d) -> %d\n", x, y, id);
    if (sConnected)
        PollState();
}

int main(int argc, char** argv) {
    (void)argc;
    char dir[512] = ".";
    if (strrchr(argv[0], '/')) {
        snprintf(dir, sizeof dir, "%s", argv[0]);
        *strrchr(dir, '/') = 0;
    }
    if (getenv("GS_DATA_DIR"))
        snprintf(dir, sizeof dir, "%s", getenv("GS_DATA_DIR"));
    if (!RaClient_Open("127.0.0.1", 55355)) {
        fprintf(stderr, "[gs] cannot open the RetroArch command socket\n");
        return 1;
    }
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0 || TTF_Init() != 0) {
        fprintf(stderr, "[gs] SDL init failed: %s\n", SDL_GetError());
        return 1;
    }
    const bool windowed = getenv("GS_WINDOWED") != NULL || SDL_GetNumVideoDisplays() < 2;
    Uint32 flags = SDL_WINDOW_SHOWN;
    int px = SDL_WINDOWPOS_UNDEFINED, py = SDL_WINDOWPOS_UNDEFINED;
    if (!windowed) {
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_BORDERLESS;
        px = SDL_WINDOWPOS_UNDEFINED_DISPLAY(1);
        py = SDL_WINDOWPOS_UNDEFINED_DISPLAY(1);
    }
    /* the sway config moves windows titled "...Bottom Screen" to DSI-1 */
    sWin = SDL_CreateWindow("Golden Sun - Bottom Screen", px, py, W, H, flags);
    if (!sWin) {
        fprintf(stderr, "[gs] window failed: %s\n", SDL_GetError());
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
        fprintf(stderr, "[gs] cannot load font %s: %s\n", fontPath, TTF_GetError());
        return 1;
    }
    sTouchFd = OpenBottomTouch();

    /* QA: GS_TEST_TAPS="ms,x,y;ms,x,y" injects taps through the real tap handler */
    struct { unsigned ms; int x, y; bool done; } taps[32];
    int tapCount = 0;
    if (getenv("GS_TEST_TAPS")) {
        char* spec = strdup(getenv("GS_TEST_TAPS"));
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
                fprintf(stderr, "[gs] test tap (%d,%d)\n", taps[i].x, taps[i].y);
                OnTap(taps[i].x, taps[i].y);
            }
        const Uint32 now = SDL_GetTicks();
        if (now - lastPoll >= 150) {
            lastPoll = now;
            if (PollState()) {
                if (!sConnected)
                    fprintf(stderr, "[gs] connected to RetroArch\n");
                sConnected = true;
                silent = 0;
            } else if (++silent > 7u * (sConnected ? MAX_SILENT_SECONDS : STARTUP_GRACE_SECONDS)) {
                fprintf(stderr, "[gs] RetroArch is gone, exiting\n");
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
