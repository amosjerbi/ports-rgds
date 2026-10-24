/* pe-bottom: touch companion for Pokemon Emerald running in RetroArch's mGBA core on the RG DS.
 * Runs on the bottom panel (DSI-1). Reads party, bag and trainer data over RetroArch's network command interface
 * (READ_CORE_MEMORY, UDP 55355). Read-only. Tabs: PARTY, BAG, TRAINER. */
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pe_data.h"
#include "ra_client.h"

#define W 640
#define H 480
#define HDR_H 58
#define TAB_Y 424
#define TAB_H 56
#define MAX_SILENT_SECONDS 20
#define STARTUP_GRACE_SECONDS 180

#include "ui_common.h"

/* ---- game text -------------------------------------------------------------------------------------------------- */
static void DecodeText(char* dst, size_t cap, const uint8_t* src, size_t n) {
    size_t o = 0;
    for (size_t i = 0; i < n && src[i] != 0xFF && o + 2 < cap; i++) {
        const uint8_t c = src[i];
        char ch = '?';
        if (c >= 0xBB && c <= 0xD4) ch = (char)('A' + c - 0xBB);
        else if (c >= 0xD5 && c <= 0xEE) ch = (char)('a' + c - 0xD5);
        else if (c >= 0xA1 && c <= 0xAA) ch = (char)('0' + c - 0xA1);
        else if (c == 0x00) ch = ' ';
        else if (c == 0xAB) ch = '!';
        else if (c == 0xAC) ch = '?';
        else if (c == 0xAD) ch = '.';
        else if (c == 0xAE) ch = '-';
        else if (c == 0xB4) ch = '\'';
        else if (c == 0xB8) ch = ',';
        else if (c == 0xF0) ch = ':';
        else if (c == 0xB5 || c == 0xB6) ch = 'x';
        dst[o++] = ch;
    }
    while (o && dst[o - 1] == ' ') o--;
    dst[o] = 0;
}

static char sSpecies[SPECIES_COUNT][12], sMove[MOVE_COUNT][14], sItem[ITEM_COUNT][15];
static bool sRomLoaded;

static bool LoadRom(void) {
    static uint8_t buf[ITEM_COUNT * ITEM_REC];
    if (!RaClient_Read(ROM_SPECIES, buf, SPECIES_COUNT * 11)) return false;
    for (int i = 0; i < SPECIES_COUNT; i++) DecodeText(sSpecies[i], sizeof sSpecies[i], buf + i * 11, 10);
    if (!RaClient_Read(ROM_MOVES, buf, MOVE_COUNT * 13)) return false;
    for (int i = 0; i < MOVE_COUNT; i++) DecodeText(sMove[i], sizeof sMove[i], buf + i * 13, 12);
    if (!RaClient_Read(ROM_ITEMS, buf, ITEM_COUNT * ITEM_REC)) return false;
    for (int i = 0; i < ITEM_COUNT; i++) DecodeText(sItem[i], sizeof sItem[i], buf + i * ITEM_REC, 14);
    return true;
}

/* ---- game state ------------------------------------------------------------------------------------------------- */
typedef struct {
    bool ok;
    char nick[12], species[12];
    int speciesId, item, level, hp, maxHp, stat[5], move[4], pp[4], exp, friendship;
} Mon;
typedef struct { int id, qty; } Slot;
enum { P_ITEMS, P_KEY, P_BALLS, P_TMS, P_BERRIES, P_COUNT };
static const char* const kPocket[P_COUNT] = { "ITEMS", "KEY", "BALLS", "TM/HM", "BERRIES" };
static const int kPocketOff[P_COUNT] = { SB1_BAG_ITEMS, SB1_BAG_KEY, SB1_BAG_BALLS, SB1_BAG_TMS, SB1_BAG_BERRIES };
static const int kPocketLen[P_COUNT] = { 30, 30, 16, 64, 46 };

static struct {
    bool valid;
    int partyCount;
    Mon mon[6];
    char name[10];
    unsigned tid, hours, minutes, seconds, money, coins;
    int badgeBits, badges, owned, seen;
    Slot bag[P_COUNT][64];
    int bagCount[P_COUNT];
} sSt;

static uint16_t U16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t U32(const uint8_t* p) { return U16(p) | (uint32_t)U16(p + 2) << 16; }

static const char kOrder[24][5] = { "GAEM", "GAME", "GEAM", "GEMA", "GMAE", "GMEA", "AGEM", "AGME", "AEGM", "AEMG", "AMGE", "AMEG",
                                    "EGAM", "EGMA", "EAGM", "EAMG", "EMGA", "EMAG", "MGAE", "MGEA", "MAGE", "MAEG", "MEGA", "MEAG" };

static void DecodeMon(const uint8_t* m, Mon* out) {
    memset(out, 0, sizeof *out);
    const uint32_t pv = U32(m), key = pv ^ U32(m + 4);
    if (!pv && !key) return;
    uint8_t dec[48];
    for (int i = 0; i < 48; i += 4) {
        const uint32_t v = U32(m + 32 + i) ^ key;
        dec[i] = (uint8_t)v; dec[i + 1] = (uint8_t)(v >> 8); dec[i + 2] = (uint8_t)(v >> 16); dec[i + 3] = (uint8_t)(v >> 24);
    }
    const uint8_t* sub[4]; /* G A E M */
    for (int p = 0; p < 4; p++) {
        const char t = kOrder[pv % 24][p];
        sub[t == 'G' ? 0 : t == 'A' ? 1 : t == 'E' ? 2 : 3] = dec + p * 12;
    }
    out->speciesId = U16(sub[0]);
    out->item = U16(sub[0] + 2);
    out->exp = (int)U32(sub[0] + 4);
    out->friendship = sub[0][9];
    for (int i = 0; i < 4; i++) { out->move[i] = U16(sub[1] + i * 2); out->pp[i] = sub[1][8 + i]; }
    out->level = m[84];
    out->hp = U16(m + 86);
    out->maxHp = U16(m + 88);
    for (int i = 0; i < 5; i++) out->stat[i] = U16(m + 90 + i * 2); /* atk def spe spa spd */
    DecodeText(out->nick, sizeof out->nick, m + 8, 10);
    out->ok = out->speciesId > 0 && out->speciesId < SPECIES_COUNT && out->maxHp > 0 && out->level >= 1 && out->level <= 100;
    if (out->ok) snprintf(out->species, sizeof out->species, "%s", sSpecies[out->speciesId]);
}

static bool PollState(void) {
    uint8_t ptrs[8], cnt, party[6 * MON_SIZE], sb2[SB2_READ];
    static uint8_t sb1[SB1_LO_LEN];
    if (!RaClient_Read(ADDR_SB1_PTR, ptrs, 8)) return false;
    const uint32_t p1 = U32(ptrs), p2 = U32(ptrs + 4);
    if (p1 < 0x02000000u || p1 > 0x0203FFFFu || p2 < 0x02000000u || p2 > 0x0203FFFFu) { sSt.valid = false; return true; }
    if (!RaClient_Read(ADDR_PARTY_COUNT, &cnt, 1) || !RaClient_Read(ADDR_PARTY, party, sizeof party) ||
        !RaClient_Read(p2, sb2, sizeof sb2) || !RaClient_Read(p1 + SB1_LO, sb1, sizeof sb1))
        return false;
    uint8_t bb[2];
    if (!RaClient_Read(p1 + SB1_BADGES, bb, 2)) return false;
    const int badge = ((bb[0] | bb[1] << 8) >> 7) & 0xFF; /* flags 0x867..0x86E */
    const uint32_t key = U32(sb2 + SB2_KEY);
    sSt.partyCount = cnt > 6 ? 6 : cnt;
    for (int i = 0; i < 6; i++) {
        DecodeMon(party + i * MON_SIZE, &sSt.mon[i]);
        if (i >= sSt.partyCount) sSt.mon[i].ok = false;
    }
    DecodeText(sSt.name, sizeof sSt.name, sb2 + SB2_NAME, 8);
    sSt.tid = U16(sb2 + SB2_TID);
    sSt.hours = U16(sb2 + SB2_PLAY);
    sSt.minutes = sb2[SB2_PLAY + 2];
    sSt.seconds = sb2[SB2_PLAY + 3];
    sSt.money = U32(sb1 + (SB1_MONEY - SB1_LO)) ^ key;
    sSt.coins = U16(sb1 + (SB1_COINS - SB1_LO)) ^ (key & 0xFFFF);
    sSt.badgeBits = badge;
    sSt.badges = __builtin_popcount(badge);
    sSt.owned = sSt.seen = 0;
    for (int i = 0; i < 52; i++) {
        sSt.owned += __builtin_popcount(sb2[SB2_DEX_OWNED + i]);
        sSt.seen += __builtin_popcount(sb2[SB2_DEX_SEEN + i]);
    }
    for (int p = 0; p < P_COUNT; p++) {
        sSt.bagCount[p] = 0;
        for (int i = 0; i < kPocketLen[p]; i++) {
            const uint8_t* s = sb1 + (kPocketOff[p] - SB1_LO) + i * 4;
            const int id = U16(s), qty = U16(s + 2) ^ (int)(key & 0xFFFF);
            if (id > 0 && id < ITEM_COUNT && qty > 0 && qty < 1000)
                sSt.bag[p][sSt.bagCount[p]++] = (Slot){ id, qty };
        }
    }
    sSt.valid = sSt.partyCount > 0 && sSt.mon[0].ok && sSt.name[0] && sSt.hours < 1000;
    return true;
}

/* ---- UI --------------------------------------------------------------------------------------------------------- */
enum { TAB_PARTY, TAB_BAG, TAB_TRAINER, TAB_COUNT };
static const char* const kTabName[TAB_COUNT] = { "PARTY", "BAG", "TRAINER" };
enum { H_TAB = 100, H_POCKET = 120, H_SCROLL_UP = 90, H_SCROLL_DN = 91, H_ROW = 200 };
#define ROW_H 38
static const char* const kStatName[5] = { "Atk", "Def", "Spe", "SpA", "SpD" };

static int sTab = TAB_PARTY, sSelMon, sPocket, sScroll[P_COUNT];
static bool sConnected;

static void DrawHeader(void) {
    FillRect(0, 0, W, HDR_H, C_PANEL);
    FillRect(0, HDR_H - 2, W, 2, C_GOLD);
    Text(sFont22, 14, 14, 0, C_GOLD, "%s", sSt.name);
    Text(sFont14, 160, 8, 0, C_DIM, "MONEY");
    Text(sFont18, 160, 27, 0, C_CREAM, "$%u", sSt.money);
    Text(sFont14, 330, 8, 0, C_DIM, "BADGES");
    Text(sFont18, 330, 27, 0, C_CREAM, "%d/8", sSt.badges);
    Text(sFont14, 426, 8, 0, C_DIM, "DEX");
    Text(sFont18, 426, 27, 0, C_CREAM, "%d", sSt.owned);
    Text(sFont14, 626, 8, 2, C_DIM, "TIME");
    Text(sFont18, 626, 27, 2, C_CREAM, "%u:%02u", sSt.hours, sSt.minutes);
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

static void DrawParty(void) {
    const int y0 = HDR_H + 6, rh = 42;
    for (int i = 0; i < 6; i++) {
        const Mon* m = &sSt.mon[i];
        const int y = y0 + i * rh;
        FillRect(4, y, W - 8, rh - 4, i == sSelMon ? C_SEL : (i & 1 ? C_PANEL : C_PANEL2));
        if (i == sSelMon) FrameRect(4, y, W - 8, rh - 4, C_GOLD);
        if (!m->ok) {
            Text(sFont14, 16, y + 10, 0, C_DIM, "---");
            continue;
        }
        Text(sFont18, 14, y + 8, 0, C_CREAM, "%s", m->nick[0] ? m->nick : m->species);
        Text(sFont12, 190, y + 13, 0, C_DIM, "%s", m->species);
        Text(sFont14, 300, y + 11, 0, C_GOLD, "Lv%d", m->level);
        FillRect(358, y + 14, 150, 9, C_BG);
        const int pct = m->maxHp ? 150 * (m->hp > m->maxHp ? m->maxHp : m->hp) / m->maxHp : 0;
        FillRect(358, y + 14, pct, 9, pct * 100 / 150 > 50 ? C_GREEN : pct * 100 / 150 > 20 ? C_YELLOW : C_RED);
        Text(sFont12, 516, y + 12, 0, C_CREAM, "%d/%d", m->hp, m->maxHp);
        AddHit(4, y, W - 8, rh - 4, H_ROW + i);
    }
    const int dy = y0 + 6 * rh + 2;
    FillRect(4, dy, W - 8, TAB_Y - dy - 6, C_PANEL);
    FrameRect(4, dy, W - 8, TAB_Y - dy - 6, C_GOLD);
    const Mon* m = &sSt.mon[sSelMon];
    if (!m->ok) return;
    for (int k = 0; k < 4; k++) {
        const int mv = m->move[k];
        Text(sFont14, 14 + (k % 2) * 200, dy + 6 + (k / 2) * 20, 0, mv > 0 && mv < MOVE_COUNT ? C_CREAM : C_DIM, "%s",
             mv > 0 && mv < MOVE_COUNT ? sMove[mv] : "-");
    }
    for (int k = 0; k < 5; k++)
        Text(sFont12, 14 + k * 76, dy + 52, 0, C_GOLD, "%s %d", kStatName[k], m->stat[k]);
    Text(sFont12, 420, dy + 6, 0, C_DIM, "Holding");
    Text(sFont14, 420, dy + 22, 0, C_CREAM, "%s", m->item > 0 && m->item < ITEM_COUNT ? sItem[m->item] : "-");
    Text(sFont12, 420, dy + 44, 0, C_DIM, "EXP %d", m->exp);
}

static void DrawBag(void) {
    const int sw = (W - 8) / P_COUNT;
    for (int i = 0; i < P_COUNT; i++) {
        const bool act = i == sPocket;
        const int x = 4 + i * sw;
        FillRect(x, HDR_H + 4, sw - 4, 32, act ? C_SEL : C_PANEL2);
        FrameRect(x, HDR_H + 4, sw - 4, 32, act ? C_GOLD : C_DIM);
        Text(sFont14, x + (sw - 4) / 2, HDR_H + 11, 1, act ? C_WHITE : C_CREAM, "%s", kPocket[i]);
        AddHit(x, HDR_H + 4, sw - 4, 32, H_POCKET + i);
    }
    const int y0 = HDR_H + 42, h = TAB_Y - y0 - 4, visible = h / ROW_H, rows = sSt.bagCount[sPocket];
    int* sc = &sScroll[sPocket];
    const int maxs = rows > visible ? rows - visible : 0;
    if (*sc > maxs) *sc = maxs;
    if (*sc < 0) *sc = 0;
    const int listW = W - 52;
    if (!rows) Text(sFont18, W / 2, y0 + 40, 1, C_DIM, "Empty");
    for (int v = 0; v < visible && *sc + v < rows; v++) {
        const Slot* s = &sSt.bag[sPocket][*sc + v];
        const int y = y0 + v * ROW_H;
        FillRect(4, y, listW, ROW_H - 3, (v & 1) ? C_PANEL : C_PANEL2);
        Text(sFont18, 14, y + 7, 0, C_CREAM, "%s", sItem[s->id]);
        if (sPocket != P_KEY) Text(sFont18, listW - 8, y + 7, 2, C_GOLD, "x%d", s->qty);
    }
    const int bx = W - 44;
    FillRect(bx, y0, 40, h / 2 - 2, C_PANEL2);
    FrameRect(bx, y0, 40, h / 2 - 2, C_DIM);
    Text(sFont22, bx + 20, y0 + h / 4 - 16, 1, C_CREAM, "^");
    FillRect(bx, y0 + h / 2 + 2, 40, h / 2 - 2, C_PANEL2);
    FrameRect(bx, y0 + h / 2 + 2, 40, h / 2 - 2, C_DIM);
    Text(sFont22, bx + 20, y0 + h * 3 / 4 - 14, 1, C_CREAM, "v");
    AddHit(bx, y0, 40, h / 2 - 2, H_SCROLL_UP);
    AddHit(bx, y0 + h / 2 + 2, 40, h / 2 - 2, H_SCROLL_DN);
}

static void DrawTrainer(void) {
    Text(sFont22, 24, 84, 0, C_GOLD, "%s", sSt.name);
    Text(sFont14, 24, 116, 0, C_DIM, "Trainer ID %05u", sSt.tid);
    Text(sFont18, 24, 146, 0, C_CREAM, "Play time   %u:%02u:%02u", sSt.hours, sSt.minutes, sSt.seconds);
    Text(sFont18, 24, 176, 0, C_CREAM, "Money   $%u", sSt.money);
    Text(sFont18, 24, 206, 0, C_CREAM, "Coins   %u", sSt.coins);
    Text(sFont18, 24, 236, 0, C_CREAM, "Pokedex   %d caught   %d seen", sSt.owned, sSt.seen);
    Text(sFont14, 24, 280, 0, C_DIM, "BADGES");
    for (int i = 0; i < 8; i++) {
        const bool got = (sSt.badgeBits >> i) & 1;
        FillRect(24 + i * 74, 304, 64, 64, got ? C_RED : C_PANEL2);
        FrameRect(24 + i * 74, 304, 64, 64, got ? C_GOLD : C_DIM);
        Text(sFont22, 24 + i * 74 + 32, 322, 1, got ? C_WHITE : C_DIM, "%d", i + 1);
    }
}

static void DrawStatus(const char* l1, const char* l2) {
    FillRect(0, 0, W, H, C_BG);
    Text(sFont22, W / 2, 170, 1, C_GOLD, "POKEMON EMERALD");
    Text(sFont18, W / 2, 214, 1, C_CREAM, "%s", l1);
    Text(sFont14, W / 2, 246, 1, C_DIM, "%s", l2);
}

static void DrawFrame(void) {
    SDL_SetRenderDrawColor(sRen, C_BG.r, C_BG.g, C_BG.b, 255);
    SDL_RenderClear(sRen);
    sHitCount = 0;
    if (!sConnected) {
        DrawStatus("Waiting for RetroArch...", "Launch Pokemon Emerald (mGBA core)");
    } else if (!sSt.valid) {
        DrawStatus("Load a saved game", "Choose Continue on the title screen");
    } else {
        DrawHeader();
        switch (sTab) {
            case TAB_PARTY: DrawParty(); break;
            case TAB_BAG: DrawBag(); break;
            case TAB_TRAINER: DrawTrainer(); break;
        }
        DrawTabs();
    }
    SDL_RenderPresent(sRen);
}

static void OnTap(int x, int y) {
    const int id = HitTest(x, y);
    if (id < 0) return;
    if (id >= H_TAB && id < H_TAB + TAB_COUNT) sTab = id - H_TAB;
    else if (id >= H_POCKET && id < H_POCKET + P_COUNT) sPocket = id - H_POCKET;
    else if (id == H_SCROLL_UP) sScroll[sPocket] -= 3;
    else if (id == H_SCROLL_DN) sScroll[sPocket] += 3;
    else if (id >= H_ROW && id < H_ROW + 6) sSelMon = id - H_ROW;
    fprintf(stderr, "[pe] tap (%d,%d) -> %d\n", x, y, id);
}

static void ApplyDrag(void) {
    if (abs(sDragDy) < ROW_H / 2) return;
    if (sTab == TAB_BAG && sTy > HDR_H + 42 && sTy < TAB_Y)
        sScroll[sPocket] -= sDragDy / ROW_H ? sDragDy / ROW_H : (sDragDy > 0 ? 1 : -1);
    sDragDy = 0;
}

int main(int argc, char** argv) {
    (void)argc;
    char dir[512] = ".";
    if (strrchr(argv[0], '/')) {
        snprintf(dir, sizeof dir, "%s", argv[0]);
        *strrchr(dir, '/') = 0;
    }
    if (getenv("PE_DATA_DIR")) snprintf(dir, sizeof dir, "%s", getenv("PE_DATA_DIR"));
    if (!RaClient_Open(getenv("PE_HOST") ? getenv("PE_HOST") : "127.0.0.1", 55355)) {
        fprintf(stderr, "[pe] cannot open the RetroArch command socket\n");
        return 1;
    }
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0 || TTF_Init() != 0) {
        fprintf(stderr, "[pe] SDL init failed: %s\n", SDL_GetError());
        return 1;
    }
    const bool windowed = getenv("PE_WINDOWED") != NULL || SDL_GetNumVideoDisplays() < 2;
    Uint32 flags = SDL_WINDOW_SHOWN;
    int px = SDL_WINDOWPOS_UNDEFINED, py = SDL_WINDOWPOS_UNDEFINED;
    if (!windowed) {
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_BORDERLESS;
        px = SDL_WINDOWPOS_UNDEFINED_DISPLAY(1);
        py = SDL_WINDOWPOS_UNDEFINED_DISPLAY(1);
    }
    /* the sway config moves windows titled "...Bottom Screen" to DSI-1 */
    sWin = SDL_CreateWindow("Pokemon Emerald - Bottom Screen", px, py, W, H, flags);
    if (!sWin) {
        fprintf(stderr, "[pe] window failed: %s\n", SDL_GetError());
        return 1;
    }
    sRen = SDL_CreateRenderer(sWin, -1, SDL_RENDERER_ACCELERATED);
    if (!sRen) sRen = SDL_CreateRenderer(sWin, -1, SDL_RENDERER_SOFTWARE);
    SDL_RenderSetLogicalSize(sRen, W, H);
    SDL_ShowCursor(SDL_DISABLE);
    char fontPath[560];
    snprintf(fontPath, sizeof fontPath, "%s/DejaVuSans.ttf", dir);
    sFont22 = TTF_OpenFont(fontPath, 22);
    sFont18 = TTF_OpenFont(fontPath, 18);
    sFont14 = TTF_OpenFont(fontPath, 14);
    sFont12 = TTF_OpenFont(fontPath, 12);
    if (!sFont22 || !sFont18 || !sFont14 || !sFont12) {
        fprintf(stderr, "[pe] cannot load font %s: %s\n", fontPath, TTF_GetError());
        return 1;
    }
    sTouchFd = OpenBottomTouch();

    /* QA: PE_TEST_TAPS="ms,x,y;ms,x,y" injects taps through the real tap handler */
    struct { unsigned ms; int x, y; bool done; } taps[32];
    int tapCount = 0;
    if (getenv("PE_TEST_TAPS")) {
        char* spec = strdup(getenv("PE_TEST_TAPS"));
        for (char* tok = strtok(spec, ";"); tok && tapCount < 32; tok = strtok(NULL, ";")) {
            unsigned ms;
            int x, y;
            if (sscanf(tok, "%u,%d,%d", &ms, &x, &y) == 3) taps[tapCount++] = (typeof(taps[0])){ ms, x, y, false };
        }
        free(spec);
    }

    const Uint32 startMs = SDL_GetTicks();
    Uint32 lastPoll = 0, lastRomTry = 0;
    unsigned silent = 0;
    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            else if (e.type == SDL_FINGERDOWN && sTouchFd < 0) OnTap((int)(e.tfinger.x * W), (int)(e.tfinger.y * H));
            else if (e.type == SDL_MOUSEBUTTONDOWN && sTouchFd < 0) OnTap(e.button.x, e.button.y);
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
                OnTap(taps[i].x, taps[i].y);
            }
        const Uint32 now = SDL_GetTicks();
        if (now - lastPoll >= 500) {
            lastPoll = now;
            uint8_t code[4] = { 0 };
            if (RaClient_Read(PE_ROM_CODE_ADDR, code, 4) && memcmp(code, "BPEE", 4) == 0) {
                if (!sRomLoaded && now - lastRomTry > 1000) {
                    lastRomTry = now;
                    sRomLoaded = LoadRom();
                }
                if (sRomLoaded && PollState()) {
                    if (!sConnected) fprintf(stderr, "[pe] connected to RetroArch\n");
                    sConnected = true;
                    silent = 0;
                }
            } else if (++silent > 2u * (sConnected ? MAX_SILENT_SECONDS : STARTUP_GRACE_SECONDS)) {
                fprintf(stderr, "[pe] RetroArch is gone, exiting\n");
                break;
            }
        }
        DrawFrame();
        SDL_Delay(16);
    }
    RaClient_Close();
    if (sTouchFd >= 0) close(sTouchFd);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
