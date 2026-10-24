/* som-bottom: touch companion for Sword of Mana running in RetroArch's mGBA core on the RG DS.
 * Runs on the bottom panel (DSI-1). Reads hero and partner status over RetroArch's network command interface. Read-only. */
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "som_data.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include "ra_client.h"

#define W 640
#define H 480
#define HDR_H 58
#define TAB_Y 424
#define TAB_H 56
#define MAX_SILENT_SECONDS 20
#define STARTUP_GRACE_SECONDS 180

#include "ui_common.h"

typedef struct { int hp, maxHp, mp, maxMp, stat[5]; } Char;
static Char sCh[2];
static unsigned sLucre, sFrames;
static int sHeroLevel;
static bool sConnected, sValid;

/* item slots, in the game's own order (checked against the in-game Items / Accessories menus) */
enum { G_HEAL, G_MYST, G_COIN, G_MEAT, G_SEED, G_VEG, G_COUNT };
static const char* const kGroupName[G_COUNT] = { "HEAL", "MYSTERY", "COIN/ICON", "MEAT", "SEED/FRUIT", "VEGGIE" };
typedef struct { int slot; const char* name; } Entry;
#define E(s, n) { s, n }
static const Entry kHeal[] = { E(0, "Gumdrop"), E(1, "Chocolump"), E(2, "Honey Elixir"), E(3, "Magic Walnut"), E(4, "Prestoveggie"),
    E(5, "Stardust Herb"), E(6, "Angel Grail"), E(7, "GummiFrog") };
static const Entry kMyst[] = { E(8, "Blink Weed"), E(9, "Potent Posy"), E(10, "Tone Stone"), E(11, "Dream Reed"), E(12, "Magic Rope"),
    E(13, "Popoi's Notebook"), E(14, "Tiny Tapper"), E(15, "Amigo Whistle") };
static const Entry kCoin[] = { E(16, "Light Coin"), E(17, "Dark Coin"), E(18, "Moon Coin"), E(19, "Fire Coin"), E(20, "Water Coin"),
    E(21, "Wood Coin"), E(22, "Wind Coin"), E(23, "Earth Coin"), E(24, "Wisp Icon"), E(25, "Shade Icon"), E(26, "Luna Icon"),
    E(27, "Salamander Icon"), E(28, "Undine Icon"), E(29, "Dryad Icon"), E(30, "Jinn Icon"), E(31, "Gnome Icon") };
static const Entry kMeat[] = { E(32, "Animal Meat"), E(33, "Thin Meat"), E(34, "Insect Meat"), E(35, "Lizard Meat"), E(36, "Bird Meat"),
    E(37, "Morph Meat"), E(38, "Fish Meat"), E(39, "Magical Meat"), E(40, "Tough Meat"), E(41, "Rotten Meat"), E(42, "Demon Meat"),
    E(43, "Dragon Meat"), E(44, "Odd Meat"), E(45, "Mixed Meat"), E(46, "Spicy Meat"), E(47, "Phantom Meat") };
static const Entry kSeed[] = { E(48, "Round Seed"), E(49, "Small Seed"), E(50, "Oblong Seed"), E(51, "Long Seed"), E(52, "Crooked Seed"),
    E(53, "Flat Seed"), E(54, "Big Seed"), E(55, "Spiny Seed"), E(56, "Bellgrapes"), E(57, "Diceberry"), E(58, "Peach Puppy"),
    E(59, "Applesocks"), E(60, "Orange'opus"), E(61, "Citrisquid"), E(62, "Springanana"), E(63, "Mangolephant"), E(64, "Rocket Papaya"),
    E(65, "Loquat-Shoes"), E(66, "Boarmelon"), E(67, "Pine O'Clock"), E(68, "Rhinoloupe"), E(69, "Kittypie"), E(70, "Cherry Bomb"),
    E(71, "Fishy Fruit") };
static const Entry kVeg[] = { E(72, "Lilipods"), E(73, "Masked Potato"), E(74, "Spiny Carrot"), E(75, "Honey Onion"), E(76, "Cornflower"),
    E(77, "Dolphin Squash"), E(78, "Cabbadillo"), E(79, "Conchurnip"), E(80, "Needlettuce"), E(81, "Whalamato"), E(82, "Orcaplant"),
    E(83, "Mush-in-a-Box"), E(84, "Bumpkin"), E(85, "Garlicrown"), E(86, "Heart Mint"), E(87, "Spade Basil") };
static const Entry kAcc[] = { E(100, "BB Ring"), E(101, "Gem Ring"), E(102, "Cicada Earrings"), E(103, "Quartz Ring"), E(104, "Cobra Earrings"),
    E(105, "WhiteLight Ring"), E(106, "Fiend Fang"), E(107, "Bandit Earrings"), E(108, "Red Moon Horn"), E(109, "D-Fence Ring"),
    E(110, "Mist Pendant"), E(111, "Knight Crest"), E(112, "Gjallar Horn"), E(113, "Dragon Choker"), E(114, "Sage Stone"),
    E(115, "Cardinal Eye"), E(116, "FlameFlicker"), E(117, "Draupnir"), E(118, "General Crest"), E(119, "Dragon Ring"),
    E(120, "Rune Earrings"), E(121, "Code Bead"), E(122, "Wishbone"), E(123, "Crystal Ring"), E(134, "Brownie Ring"),
    E(135, "Bath Set"), E(156, "Belle Bell"), E(157, "Chimpfish Iris") };
#define N(a) ((int)(sizeof(a) / sizeof((a)[0])))
static const Entry* const kGroup[G_COUNT] = { kHeal, kMyst, kCoin, kMeat, kSeed, kVeg };
static const int kGroupLen[G_COUNT] = { N(kHeal), N(kMyst), N(kCoin), N(kMeat), N(kSeed), N(kVeg) };
#define ITEM_SLOTS 232
static uint16_t sItems[ITEM_SLOTS];
static uint8_t sItemBytes[ITEM_SLOTS * 2];

enum { TAB_STATUS, TAB_ITEMS, TAB_ACC, TAB_COUNT };
static const char* const kTabName[TAB_COUNT] = { "STATUS", "ITEMS", "ACCESSORIES" };
enum { H_HEAL = 140, H_TAB = 100, H_GROUP = 120, H_SCROLL_UP = 90, H_SCROLL_DN = 91 };
#define ROW_H 34
static int sTab = TAB_STATUS, sGroup, sScroll[G_COUNT + 1];
static const char* const kWho[2] = { "HERO", "PARTNER" };

static uint16_t U16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }

static bool PollState(void) {
    uint8_t v[2][8], st[2 * STATS_STRIDE], l[4], t[4], lv[2];
    if (!RaClient_Read(ADDR_VITALS, v[0], 8) || !RaClient_Read(ADDR_VITALS2, v[1], 8) || !RaClient_Read(ADDR_STATS, st, sizeof st) ||
        !RaClient_Read(ADDR_ITEMS, sItemBytes, sizeof sItemBytes) || !RaClient_Read(ADDR_LUCRE, l, 4) || !RaClient_Read(ADDR_FRAMES, t, 4) || !RaClient_Read(ADDR_HERO_LEVEL, lv, 2))
        return false;
    for (int c = 0; c < 2; c++) {
        sCh[c].hp = U16(v[c]);
        sCh[c].maxHp = U16(v[c] + 2);
        sCh[c].mp = U16(v[c] + 4);
        sCh[c].maxMp = U16(v[c] + 6);
        }
    sLucre = (unsigned)(l[0] | l[1] << 8 | l[2] << 16 | (unsigned)l[3] << 24);
    sFrames = (unsigned)(t[0] | t[1] << 8 | t[2] << 16 | (unsigned)t[3] << 24);
    sHeroLevel = U16(lv);
    for (int i = 0; i < ITEM_SLOTS; i++) sItems[i] = U16(sItemBytes + i * 2);
    sValid = sCh[0].maxHp > 0 && sCh[0].maxHp < 10000 && sCh[0].hp <= sCh[0].maxHp && sHeroLevel >= 1 && sHeroLevel <= 99;
    return true;
}

static void Bar(int x, int y, int w, int cur, int max, Col c) {
    FillRect(x - 1, y - 1, w + 2, 12, C_TRIM);
    FillRect(x, y, w, 10, C_FIELD);
    if (max > 0) FillRect(x, y, w * (cur > max ? max : cur) / max, 10, c);
}

static void DrawCard(int c, int x) {
    const int w = 308, y0 = HDR_H + 8;
    FillRect(x, y0, w, 150, C_PANEL);
    FrameRect(x, y0, w, 150, C_TRIM);
    FrameRect(x + 1, y0 + 1, w - 2, 148, C_PANEL2);
    Text(sFont22, x + 14, y0 + 10, 0, C_GOLD, "%s", kWho[c]);
    if (c == 0) Text(sFont18, x + w - 14, y0 + 14, 2, C_CREAM, "Lv %d", sHeroLevel);
    Text(sFont14, x + 14, y0 + 52, 0, C_CREAM, "HP");
    Text(sFont18, x + w - 14, y0 + 48, 2, C_CREAM, "%d / %d", sCh[c].hp, sCh[c].maxHp);
    Bar(x + 14, y0 + 76, w - 28, sCh[c].hp, sCh[c].maxHp, C_GREEN);
    Text(sFont14, x + 14, y0 + 98, 0, C_CREAM, "MP");
    Text(sFont18, x + w - 14, y0 + 94, 2, C_CREAM, "%d / %d", sCh[c].mp, sCh[c].maxMp);
    Bar(x + 14, y0 + 122, w - 28, sCh[c].mp, sCh[c].maxMp, C_BLUE);
}

/* the eight recovery items, always visible under the HP / MP cards */
static void DrawHeal(void) {
    const int y0 = HDR_H + 166, w = W - 16, ph = 4 * 32 + 40;
    FillRect(8, y0, w, ph, C_PANEL);
    FrameRect(8, y0, w, ph, C_TRIM);
    FrameRect(9, y0 + 1, w - 2, ph - 2, C_PANEL2);
    Text(sFont14, 20, y0 + 8, 0, C_GOLD, "HEALING ITEMS");
    /* left column heals HP, right column heals MP (kHeal order: Gumdrop, Chocolump, Honey Elixir, Magic Walnut, Prestoveggie, Stardust Herb, Angel Grail, GummiFrog) */
    static const int kCol[8] = { 0, 0, 0, 1, 1, 1, 0, 1 };
    static const int kRow[8] = { 0, 1, 2, 0, 1, 2, 3, 3 };
    Text(sFont14, 20 + 150, y0 + 8, 0, C_DIM, "HP");
    Text(sFont14, 20 + w / 2, y0 + 8, 0, C_DIM, "MP");
    for (int i = 0; i < 8; i++) {
        const int q = sItems[kHeal[i].slot];
        const int x = 14 + kCol[i] * (w / 2);
        const int yy = y0 + 32 + kRow[i] * 32;
        FillRect(x, yy, w / 2 - 12, 28, q ? C_PANEL2 : C_PANEL);
        FrameRect(x, yy, w / 2 - 12, 28, q ? C_TRIM : C_PANEL2);
        Text(sFont18, x + 10, yy + 3, 0, q ? C_CREAM : C_DIM, "%s", kHeal[i].name);
        Text(sFont18, x + w / 2 - 22, yy + 3, 2, q ? C_GOLD : C_DIM, "x%d", q);
        AddHit(x, yy, w / 2 - 12, 28, H_HEAL + i);
    }
}

static void DrawTabs(void) {
    FillRect(0, TAB_Y - 3, W, H - TAB_Y + 3, C_PANEL);
    FillRect(0, TAB_Y - 3, W, 3, C_TRIM);
    const int tw = W / TAB_COUNT;
    for (int t = 0; t < TAB_COUNT; t++) {
        const bool act = t == sTab;
        FillRect(t * tw + 4, TAB_Y + 4, tw - 8, TAB_H - 8, act ? C_PANEL2 : C_PANEL);
        FrameRect(t * tw + 4, TAB_Y + 4, tw - 8, TAB_H - 8, act ? C_GOLD : C_TRIM);
        Text(sFont18, t * tw + tw / 2, TAB_Y + 17, 1, act ? C_GOLD : C_CREAM, "%s", kTabName[t]);
        AddHit(t * tw, TAB_Y, tw, TAB_H, H_TAB + t);
    }
}

static const Entry* CurList(int* len, int** scroll) {
    if (sTab == TAB_ACC) { *len = N(kAcc); *scroll = &sScroll[G_COUNT]; return kAcc; }
    *len = kGroupLen[sGroup];
    *scroll = &sScroll[sGroup];
    return kGroup[sGroup];
}

static void DrawListTab(void) {
    int y0 = HDR_H + 6;
    if (sTab == TAB_ITEMS) {
        const int sw = (W - 8) / G_COUNT;
        for (int g = 0; g < G_COUNT; g++) {
            const bool act = g == sGroup;
            const int x = 4 + g * sw;
            FillRect(x, y0, sw - 4, 30, act ? C_PANEL2 : C_PANEL);
            FrameRect(x, y0, sw - 4, 30, act ? C_GOLD : C_TRIM);
            Text(sFont12, x + (sw - 4) / 2, y0 + 8, 1, act ? C_GOLD : C_CREAM, "%s", kGroupName[g]);
            AddHit(x, y0, sw - 4, 30, H_GROUP + g);
        }
        y0 += 36;
    }
    int len, *sc;
    const Entry* list = CurList(&len, &sc);
    const int h = TAB_Y - 6 - y0, visible = h / ROW_H, maxs = len > visible ? len - visible : 0;
    if (*sc > maxs) *sc = maxs;
    if (*sc < 0) *sc = 0;
    const int listW = W - 52;
    int owned = 0;
    for (int i = 0; i < len; i++) owned += sItems[list[i].slot] > 0;
    for (int v = 0; v < visible && *sc + v < len; v++) {
        const Entry* e = &list[*sc + v];
        const int q = sItems[e->slot], y = y0 + v * ROW_H;
        FillRect(4, y, listW, ROW_H - 3, q ? C_PANEL : C_PANEL2);
        FrameRect(4, y, listW, ROW_H - 3, q ? C_TRIM : C_PANEL2);
        Text(sFont18, 14, y + 6, 0, q ? C_CREAM : C_DIM, "%s", e->name);
        if (q) Text(sFont18, listW - 8, y + 6, 2, C_GOLD, "x%d", q);
    }
    Text(sFont12, 14, TAB_Y - 20, 0, C_CREAM, "%d / %d owned", owned, len);
    const int bx = W - 44;
    FillRect(bx, y0, 40, h / 2 - 2, C_PANEL);
    FrameRect(bx, y0, 40, h / 2 - 2, C_TRIM);
    Text(sFont22, bx + 20, y0 + h / 4 - 16, 1, C_CREAM, "^");
    FillRect(bx, y0 + h / 2 + 2, 40, h / 2 - 2, C_PANEL);
    FrameRect(bx, y0 + h / 2 + 2, 40, h / 2 - 2, C_TRIM);
    Text(sFont22, bx + 20, y0 + h * 3 / 4 - 14, 1, C_CREAM, "v");
    AddHit(bx, y0, 40, h / 2 - 2, H_SCROLL_UP);
    AddHit(bx, y0 + h / 2 + 2, 40, h / 2 - 2, H_SCROLL_DN);
}

static void DrawFrame(void) {
    for (int y = 0; y < H; y += 4) /* the title / save screens' blue sky fading to black */
        FillRect(0, y, W, 4, (Col){ (uint8_t)(C_SKY.r * (H - y) / H), (uint8_t)(C_SKY.g * (H - y) / H), (uint8_t)(C_SKY.b * (H - y) / H) });
    sHitCount = 0;
    if (!sConnected) {
        Text(sFont22, W / 2, 170, 1, C_GOLD, "SWORD OF MANA");
        Text(sFont18, W / 2, 214, 1, C_CREAM, "Waiting for RetroArch...");
    } else if (!sValid) {
        Text(sFont22, W / 2, 170, 1, C_GOLD, "SWORD OF MANA");
        Text(sFont18, W / 2, 214, 1, C_CREAM, "Load a saved game");
    } else {
        FillRect(0, 0, W, HDR_H, C_PANEL);
        FillRect(0, HDR_H - 3, W, 3, C_TRIM);
        Text(sFont22, 14, 14, 0, C_GOLD, "SWORD OF MANA");
        Text(sFont18, 330, 17, 0, C_CREAM, "%u lucre", sLucre);
        const unsigned s = sFrames / 60;
        Text(sFont18, 626, 17, 2, C_CREAM, "%u:%02u:%02u", s / 3600, (s / 60) % 60, s % 60);
        if (sTab == TAB_STATUS) {
            DrawCard(0, 8);
            DrawCard(1, 324);
            DrawHeal();
        } else {
            DrawListTab();
        }
        DrawTabs();
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
enum { PAD_B = 0, PAD_START = 3, PAD_LEFT = 6, PAD_RIGHT = 7, PAD_A = 8 };

/* 0x02013F40 is the menu state: 0 field, 7 top ring, 6 category ring, 11 item list, 3 target prompt */
enum { MS_FIELD = 0, MS_TARGET = 3, MS_CATEGORY = 6, MS_TOP = 7, MS_LIST = 11 };
static int MenuState(void) {
    uint8_t f = 0xFF;
    return RaClient_Read(0x02013F40u, &f, 1) ? f : -1;
}
static bool WaitMenu(int want) {
    for (int n = 0; n < 16; n++) {
        if (MenuState() == want) {
            SDL_Delay(450); /* the ring animates in and ignores input until it settles */
            return true;
        }
        SDL_Delay(120);
    }
    return false;
}
static void CloseMenu(void) {
    for (int n = 0; n < 8 && MenuState() != MS_FIELD; n++) Pad(PAD_B, 350);
}

/* Opens Items > Recovery > the item > the hero, the same way a player would, then closes the menu again. */
static void UseHealItem(int i) {
    if (sItems[kHeal[i].slot] <= 0) return;
    int idx = 0;
    for (int j = 0; j < i; j++) if (sItems[kHeal[j].slot] > 0) idx++;
    CloseMenu();
    Pad(PAD_START, 100);
    if (!WaitMenu(MS_TOP)) return;
    Pad(PAD_A, 100);
    if (WaitMenu(MS_CATEGORY)) {
        uint16_t cat = 0;
        for (int n = 0; n < 4 && RaClient_Read(0x02013EC4u, &cat, 2) && cat != 0; n++) Pad(PAD_LEFT, 250);
        Pad(PAD_A, 100);
        if (WaitMenu(MS_LIST)) {
            /* the list remembers its last cursor, so walk to the wanted entry using the live cursor */
            for (int n = 0; n < 12; n++) {
                uint16_t cur = 0xFFFF;
                if (!RaClient_Read(0x02013EC4u, &cur, 2) || cur == idx) break;
                Pad(cur < idx ? PAD_RIGHT : PAD_LEFT, 250);
            }
            Pad(PAD_A, 100);
            if (WaitMenu(MS_TARGET)) Pad(PAD_A, 900);
        }
    }
    CloseMenu();
}

static void OnTap(int x, int y) {
    const int id = HitTest(x, y);
    int len, *sc;
    CurList(&len, &sc);
    if (id >= H_TAB && id < H_TAB + TAB_COUNT) sTab = id - H_TAB;
    else if (id >= H_GROUP && id < H_GROUP + G_COUNT) sGroup = id - H_GROUP;
    else if (id >= H_HEAL && id < H_HEAL + 8 && sTab == TAB_STATUS) UseHealItem(id - H_HEAL);
    else if (id == H_SCROLL_UP) *sc -= 4;
    else if (id == H_SCROLL_DN) *sc += 4;
}

static void ApplyDrag(void) {
    if (abs(sDragDy) < ROW_H / 2) return;
    int len, *sc;
    CurList(&len, &sc);
    if (sTab != TAB_STATUS && sTy > HDR_H) *sc -= sDragDy / ROW_H ? sDragDy / ROW_H : (sDragDy > 0 ? 1 : -1);
    sDragDy = 0;
}

int main(int argc, char** argv) {
    (void)argc;
    char dir[512] = ".";
    if (strrchr(argv[0], '/')) {
        snprintf(dir, sizeof dir, "%s", argv[0]);
        *strrchr(dir, '/') = 0;
    }
    if (getenv("SOM_DATA_DIR"))
        snprintf(dir, sizeof dir, "%s", getenv("SOM_DATA_DIR"));
    if (!RaClient_Open(getenv("SOM_HOST") ? getenv("SOM_HOST") : "127.0.0.1", 55355)) {
        fprintf(stderr, "[som] cannot open the RetroArch command socket\n");
        return 1;
    }
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0 || TTF_Init() != 0) {
        fprintf(stderr, "[som] SDL init failed: %s\n", SDL_GetError());
        return 1;
    }
    const bool windowed = getenv("SOM_WINDOWED") != NULL || SDL_GetNumVideoDisplays() < 2;
    Uint32 flags = SDL_WINDOW_SHOWN;
    int px = SDL_WINDOWPOS_UNDEFINED, py = SDL_WINDOWPOS_UNDEFINED;
    if (!windowed) {
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_BORDERLESS;
        px = SDL_WINDOWPOS_UNDEFINED_DISPLAY(1);
        py = SDL_WINDOWPOS_UNDEFINED_DISPLAY(1);
    }
    /* the sway config moves windows titled "...Bottom Screen" to DSI-1 */
    sWin = SDL_CreateWindow("Sword of Mana - Bottom Screen", px, py, W, H, flags);
    if (!sWin) {
        fprintf(stderr, "[som] window failed: %s\n", SDL_GetError());
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
        fprintf(stderr, "[som] cannot load font %s: %s\n", fontPath, TTF_GetError());
        return 1;
    }
    sTouchFd = OpenBottomTouch();

    /* QA: SOM_TEST_TAPS="ms,x,y;ms,x,y" injects taps through the real tap handler */
    struct { unsigned ms; int x, y; bool done; } taps[32];
    int tapCount = 0;
    if (getenv("SOM_TEST_TAPS")) {
        char* spec = strdup(getenv("SOM_TEST_TAPS"));
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
                fprintf(stderr, "[som] test tap (%d,%d)\n", taps[i].x, taps[i].y);
                OnTap(taps[i].x, taps[i].y);
            }
        const Uint32 now = SDL_GetTicks();
        if (now - lastPoll >= 200) {
            lastPoll = now;
            uint8_t code[4] = { 0 };
            if (RaClient_Read(SOM_ROM_CODE_ADDR, code, 4) && memcmp(code, "AVSE", 4) == 0 && PollState()) {
                if (!sConnected)
                    fprintf(stderr, "[som] connected to RetroArch\n");
                sConnected = true;
                silent = 0;
            } else if (++silent > 5u * (sConnected ? MAX_SILENT_SECONDS : STARTUP_GRACE_SECONDS)) {
                fprintf(stderr, "[som] RetroArch is gone, exiting\n");
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
