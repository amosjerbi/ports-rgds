/* Golden Sun: The Lost Age (USA/Europe) companion for the Anystream RG DS: runs on the bottom panel next to RetroArch's mGBA core on the top panel.
 * Reads and writes the game's RAM over RetroArch's network command interface (UDP 55355).
 * Tabs: PARTY (stats of the eight adventurers, tap HP / PP to refill, tap a Psynergy to cast it from the pause menu),
 * DJINN (all 28 Djinn and who holds them), TOOLS (coins, heal). */
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "gsl_data.h"
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
    bool joined; /* a real character block (level and HP make sense) */
    bool inParty; /* in the active party list */
    int level, maxHp, maxPp, hp, pp, attack, defense, agility, luck;
    int power[ELEMENT_COUNT], resist[ELEMENT_COUNT];
    uint32_t djinnOwned[ELEMENT_COUNT], djinnSet[ELEMENT_COUNT];
    int setCount[ELEMENT_COUNT], ownedCount[ELEMENT_COUNT];
    uint32_t exp;
} Char;
typedef struct {
    bool valid;
    Char ch[PARTY_COUNT];
    int order[PARTY_COUNT], orderCount; /* active party, in game order */
    uint32_t coins;
} State;
static State sSt;
static bool sConnected, sRomOk;
static bool sBusy;
static const char* sBusyMsg = "";
static void DrawFrame(void);

static int U16(const uint8_t* b, int o) { return b[o] | (b[o + 1] << 8); }
static uint32_t U32(const uint8_t* b, int o) { return (uint32_t)(b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | ((uint32_t)b[o + 3] << 24)); }

static bool PollState(void) {
    static uint8_t blk[PARTY_COUNT * PARTY_STRIDE];
    uint8_t coins[4], list[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
    if (!RaClient_Read(RAM_PARTY, blk, sizeof blk))
        return false;
    if (!sRomOk) {
        uint8_t code[4] = { 0 };
        if (RaClient_Read(GSL_ROM_CODE_ADDR, code, 4) && memcmp(code, "AGFE", 4) == 0) {
            sRomOk = true;
            fprintf(stderr, "[gsl] Golden Sun: The Lost Age (USA/Europe) detected\n");
        } else if (code[0] && memcmp(code, "AGFE", 4) != 0) {
            fprintf(stderr, "[gsl] not The Lost Age (USA/Europe): %.4s\n", code);
        }
    }
    RaClient_Read(RAM_PARTY_LIST, list, 4);
    for (int i = 0; i < PARTY_COUNT; i++) {
        const uint8_t* b = blk + i * PARTY_STRIDE;
        Char* c = &sSt.ch[i];
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
        c->joined = c->level >= 1 && c->level <= 99 && c->maxHp > 0 && c->maxHp < 3000 && c->hp <= c->maxHp;
        c->inParty = false;
    }
    sSt.orderCount = 0;
    for (int k = 0; k < 4 && list[k] < PARTY_COUNT; k++) {
        sSt.order[sSt.orderCount++] = list[k];
        sSt.ch[list[k]].inParty = true;
    }
    sSt.valid = sRomOk && sSt.orderCount > 0 && sSt.ch[sSt.order[0]].joined;
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
/* Psynergy usable from the pause menu                                                                                */
/* ------------------------------------------------------------------------------------------------------------------ */
/* The Psynergy screen builds its list in IWRAM: u16 ids (high byte = flags) from 0x03002944, the entry count at 0x03002994.
 * Names are drawn from compressed text and are not in RAM, so the table below was filled in from the game's own menu. */
enum { PSY_FIELD, PSY_ONE, PSY_PARTY };
typedef struct { int id; const char* name; int cost; int kind; } PsyInfo;
static const PsyInfo kPsy[] = {
    { 0x5A, "Cure", 3, PSY_ONE },        { 0x5B, "Cure Well", 7, PSY_ONE },  { 0x5C, "Potent Cure", 10, PSY_ONE },
    { 0x65, "Revive", 15, PSY_ONE },     { 0x5D, "Ply", 4, PSY_ONE },        { 0x5E, "Ply Well", 8, PSY_ONE },
    { 0x5F, "Pure Ply", 12, PSY_ONE },   { 0x60, "Wish", 9, PSY_PARTY },     { 0x61, "Wish Well", 13, PSY_PARTY },
    { 0x62, "Pure Wish", 20, PSY_PARTY }, { 0x63, "Cure Poison", 2, PSY_ONE }, { 0x94, "Catch", 1, PSY_FIELD },
    { 0x93, "Carry", 2, PSY_FIELD },     { 0x8C, "Move", 2, PSY_FIELD },     { 0x95, "Retreat", 8, PSY_FIELD },
    { 0x4E, "Whirlwind", 5, PSY_FIELD }, { 0x91, "Halt", 2, PSY_FIELD },     { 0x8E, "Force", 2, PSY_FIELD },
    { 0x8D, "Mind Read", 1, PSY_FIELD }, { 0x90, "Reveal", 1, PSY_FIELD },   { 0x18, "Frost", 5, PSY_FIELD },
    { 0x92, "Cloak", 1, PSY_FIELD },     { 0x8F, "Lift", 2, PSY_FIELD },     { 0x21, "Douse", 5, PSY_FIELD },
};
#define PSY_KNOWN ((int)(sizeof kPsy / sizeof kPsy[0]))
#define PSY_MAX 16
typedef struct { int id; } PsyEntry;
static PsyEntry sPsy[PARTY_COUNT][PSY_MAX];
static int sPsyCount[PARTY_COUNT];
static bool sPsyScanned;
static int sPsyLevels;
static int sPsySel = -1; /* spell waiting for a target, or -1 */

static const PsyInfo* PsyLookup(int id) {
    static PsyInfo unk;
    static char nm[24];
    for (int i = 0; i < PSY_KNOWN; i++)
        if (kPsy[i].id == id) return &kPsy[i];
    snprintf(nm, sizeof nm, "Psynergy %02X", id);
    unk = (PsyInfo){ id, nm, 0, PSY_FIELD };
    return &unk;
}

/* RetroArch's network gamepad (network_remote, UDP 55400): the companion drives the game's own pause menu. */
static int sRemoteFd = -1;
/* RetroArch's remote-pad socket only answers on the device's LAN address, not on loopback */
static const char* PadHost(void) {
    static char ip[64];
    if (getenv("GSL_HOST")) return getenv("GSL_HOST");
    if (!ip[0]) {
        struct ifaddrs *ifa, *p;
        snprintf(ip, sizeof ip, "127.0.0.1");
        if (getifaddrs(&ifa) == 0) {
            for (p = ifa; p; p = p->ifa_next)
                if (p->ifa_addr && p->ifa_addr->sa_family == AF_INET && !(p->ifa_flags & IFF_LOOPBACK)) {
                    inet_ntop(AF_INET, &((struct sockaddr_in*)p->ifa_addr)->sin_addr, ip, sizeof ip);
                    break;
                }
            freeifaddrs(ifa);
        }
    }
    return ip;
}
static void Pad(int retropadId, int wait) {
    if (sRemoteFd < 0) sRemoteFd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sRemoteFd < 0) return;
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons(55400);
    inet_pton(AF_INET, PadHost(), &a.sin_addr);
    for (int st = 1; st >= 0; st--) {
        uint32_t pkt[5] = { 0, 1, 0, (uint32_t)retropadId, (uint32_t)st };
        const ssize_t r = sendto(sRemoteFd, pkt, sizeof pkt, 0, (struct sockaddr*)&a, sizeof a);
        (void)r;
        SDL_Delay(110);
    }
    SDL_Delay(wait);
    SDL_PumpEvents();
}
enum { PAD_B = 0, PAD_DOWN = 5, PAD_LEFT = 6, PAD_RIGHT = 7, PAD_A = 8, PAD_R = 11 };

/* 0x03000AF0 holds a pointer into the IWRAM stack that depends on how deep in the pause menu the game is */
enum { SCR_FIELD = 0x03007E34, SCR_MENU = 0x03007DF8, SCR_WHOSE = 0x03007D8C, SCR_LIST = 0x03007D50 };
static int Screen(void) {
    uint8_t b[4];
    if (!RaClient_Read(0x03000AF0u, b, 4)) return -1;
    return (int)U32(b, 0);
}
static bool WaitScreen(int want) {
    for (int n = 0; n < 25; n++) {
        if (Screen() == want) {
            SDL_Delay(450); /* the window slides in and ignores input until it settles */
            return true;
        }
        SDL_Delay(100);
    }
    return false;
}
static void CloseMenu(void) {
    for (int n = 0; n < 8 && Screen() != SCR_FIELD; n++) Pad(PAD_B, 500);
}

/* A > Psynergy > "Whose Psynergy?" > the first adventurer's list */
static bool OpenPsyList(void) {
    CloseMenu();
    fprintf(stderr, "[gsl] screen before menu: %d\n", Screen());
    Pad(PAD_A, 100);
    if (!WaitScreen(SCR_MENU)) return false;
    Pad(PAD_A, 100); /* the menu opens on Psynergy */
    if (!WaitScreen(SCR_WHOSE)) return false;
    return true;
}

static void ScanPsy(void) {
    sBusy = true;
    sBusyMsg = "Reading Psynergy...";
    DrawFrame();
    for (int i = 0; i < PARTY_COUNT; i++) sPsyCount[i] = 0;
    if (OpenPsyList()) {
        Pad(PAD_A, 100); /* the first party member's list */
        if (WaitScreen(SCR_LIST)) {
            for (int pos = 0; pos < sSt.orderCount; pos++) {
                const int who = sSt.order[pos];
                if (pos) Pad(PAD_R, 700);
                SDL_Delay(300);
                uint8_t cnt = 0, ids[PSY_MAX * 2];
                if (!RaClient_Read(0x03002994u, &cnt, 1) || !RaClient_Read(0x03002944u, ids, sizeof ids)) continue;
                if (cnt > PSY_MAX) cnt = PSY_MAX;
                for (int i = 0; i < cnt; i++) sPsy[who][i].id = ids[i * 2];
                sPsyCount[who] = cnt;
            }
            sPsyScanned = true;
            sPsyLevels = 0;
            for (int i = 0; i < PARTY_COUNT; i++) sPsyLevels += sSt.ch[i].level + (sSt.ch[i].inParty ? 100 * i : 0);
        }
    }
    CloseMenu();
    sBusy = false;
}

/* A > Psynergy > the adventurer > the spell (lists show 5 per page; Right turns the page) > the target for healing spells */
static int PartyPos(int slot) {
    for (int k = 0; k < sSt.orderCount; k++)
        if (sSt.order[k] == slot) return k;
    return 0;
}

/* `who` and `target` are character slots; the menu addresses them by their place in the active party */
static void CastPsy(int who, int idx, int target) {
    const PsyInfo* pi = PsyLookup(sPsy[who][idx].id);
    const int whoPos = PartyPos(who), targetPos = PartyPos(target);
    sBusy = true;
    sBusyMsg = "Casting Psynergy...";
    DrawFrame();
    if (OpenPsyList()) {
        for (int n = 0; n < whoPos; n++) Pad(PAD_RIGHT, 300);
        Pad(PAD_A, 100);
        if (WaitScreen(SCR_LIST)) {
            for (int n = 0; n < idx / 5; n++) Pad(PAD_RIGHT, 450);
            for (int n = 0; n < idx % 5; n++) Pad(PAD_DOWN, 300);
            Pad(PAD_A, 700);
            if (pi->kind == PSY_ONE) {
                for (int n = 0; n < targetPos; n++) Pad(PAD_RIGHT, 300);
                Pad(PAD_A, 1200);
            } else {
                SDL_Delay(900);
            }
        }
    }
    CloseMenu();
    sBusy = false;
}

/* ------------------------------------------------------------------------------------------------------------------ */
/* UI: the game's own blue menu look (white-framed blue windows, pink selection, small caption chips)                  */
/* ------------------------------------------------------------------------------------------------------------------ */
enum { TAB_PARTY, TAB_DJINN, TAB_PSY, TAB_COUNT };
static const char* kTabName[TAB_COUNT] = { "Party", "Djinn", "Psynergy" };
enum {
    H_TAB = 100,
    H_CHAR = 110, /* + member */
    H_HP = 120,
    H_PP = 121,
    H_COINS = 122,
    H_PSY_SCAN = 123,
    H_TARGET_BACK = 124,
    H_PSY = 140,    /* + spell */
    H_TARGET = 160, /* + member */
    H_HEAL_ALL = 130,
    H_COINS_10K = 131,
    H_COINS_100K = 132,
    H_COINS_MAX = 133
};
static int sTab = TAB_PARTY, sMember = 4;
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
static const Col C_GREY = { 120, 140, 200 };
static const Col kBg = { 4, 14, 112 }, kPanel = { 12, 38, 158 }, kPanelDk = { 6, 24, 112 }, kEdge = { 0, 0, 48 },
                 kPink = { 226, 100, 172 }, kPinkDk = { 150, 44, 108 }, kLbl = { 150, 176, 236 }, kCoin = { 255, 205, 70 },
                 kHpGreen = { 90, 214, 60 }, kPpBlue = { 40, 160, 240 }, kTrack = { 8, 18, 76 }, kOrange = { 255, 150, 30 };
/* the adepts keep their element in the game's slot order: Isaac / Felix Venus, Garet / Jenna Mars, Ivan / Sheba Jupiter, Mia / Piers Mercury */
static const int kBaseElement[PARTY_COUNT] = { 0, 2, 3, 1, 0, 2, 3, 1 };

static void DrawBar(int x, int y, int w, int h, int cur, int max, Col c) {
    FillRect(x, y, w, h, kTrack);
    if (max > 0)
        FillRect(x, y, (int)((long)w * (cur > max ? max : cur) / max), h, c);
    FrameRect(x, y, w, h, kEdge);
}

/* the menu's window: dark outline, white frame, blue fill */
static void Window(int x, int y, int w, int h) {
    FillRect(x, y, w, h, kEdge);
    FillRect(x + 1, y + 1, w - 2, h - 2, C_WHITE);
    FillRect(x + 3, y + 3, w - 6, h - 6, kPanel);
}

/* the small caption on a window's top-left corner ("STATS", "ITEMS") */
static void Chip(int x, int y, const char* label) {
    const int w = TextWidth(sFont12, label) + 14;
    FillRect(x, y, w, 15, kPinkDk);
    FillRect(x + 1, y + 1, w - 2, 13, kEdge);
    Text(sFont12, x + w / 2, y + 1, 1, C_WHITE, "%s", label);
}

static void DrawTabs(void) {
    Window(8, 388, W - 16, 86);
    const int tw = (W - 28) / TAB_COUNT;
    for (int t = 0; t < TAB_COUNT; t++) {
        const int x = 14 + t * tw, y = 394, h = 74;
        if (t == sTab) FillRect(x, y, tw, h, kPink);
        else if (t > 0) FillRect(x, y + 6, 2, h - 12, kPanelDk);
        Text(sFont18, x + tw / 2, y + h / 2 - 12, 1, C_WHITE, "%s", kTabName[t]);
        AddHit(x, y, tw, h, H_TAB + t);
    }
}

/* left: the active party first, then the others that have joined (only the active party can cast Psynergy) */
static int ListSlots(int* out, bool partyOnly) {
    int n = 0;
    for (int k = 0; k < sSt.orderCount; k++) out[n++] = sSt.order[k];
    if (!partyOnly)
        for (int i = 0; i < PARTY_COUNT; i++)
            if (sSt.ch[i].joined && !sSt.ch[i].inParty) out[n++] = i;
    return n;
}

static void DrawPartyList(bool partyOnly) {
    Window(8, 6, 176, 374);
    int slots[PARTY_COUNT];
    const int n = ListSlots(slots, partyOnly);
    const int rh = n <= 5 ? 64 : 360 / n;
    for (int k = 0; k < n; k++) {
        const int i = slots[k];
        const Char* c = &sSt.ch[i];
        const int y = 14 + k * rh, h = rh - 4;
        FillRect(13, y, 166, h, i == sMember ? kPink : kPanelDk);
        Text(sFont18, 22, y + 3, 0, c->inParty ? C_WHITE : kLbl, "%s", kCharName[i]);
        Text(sFont12, 172, y + 7, 2, C_WHITE, "L%d", c->level);
        if (h >= 56) {
            DrawBar(22, y + 32, 148, 6, c->hp, c->maxHp, c->hp * 4 < c->maxHp ? kOrange : kHpGreen);
            Text(sFont12, 170, y + 41, 2, i == sMember ? C_WHITE : kLbl, "%d/%d", c->hp, c->maxHp);
        } else {
            DrawBar(22, y + h - 14, 148, 6, c->hp, c->maxHp, c->hp * 4 < c->maxHp ? kOrange : kHpGreen);
        }
        AddHit(13, y, 166, h, H_CHAR + i);
    }
}

static void DrawParty(void) {
    DrawPartyList(false);
    const Char* c = &sSt.ch[sMember];
    /* top: who is selected, with HP and PP */
    Window(192, 6, 440, 66);
    const Col ec = kElementCol[kBaseElement[sMember]];
    FillRect(204, 14, 48, 50, kEdge);
    FillRect(206, 16, 44, 46, kPanelDk);
    Text(sFont22, 228, 26, 1, ec, "%c", kCharName[sMember][0]);
    Text(sFont18, 264, 14, 0, C_WHITE, "%s", kCharName[sMember]);
    const int nw = TextWidth(sFont18, kCharName[sMember]);
    Text(sFont14, 264 + nw + 10, 18, 0, C_WHITE, "L%d", c->level);
    {
        static const char* const kBase[ELEMENT_COUNT] = { "Venus", "Mercury", "Mars", "Jupiter" };
        Text(sFont12, 264 + nw + 52, 20, 0, kLbl, "%s Adept", kBase[kBaseElement[sMember]]);
    }
    Text(sFont12, 264, 44, 0, kPink, "HP");
    DrawBar(286, 48, 112, 8, c->hp, c->maxHp, c->hp * 4 < c->maxHp ? kOrange : kHpGreen);
    Text(sFont12, 402, 42, 0, C_WHITE, "%d/%d", c->hp, c->maxHp);
    AddHit(258, 36, 190, 30, H_HP);
    Text(sFont12, 468, 44, 0, kPink, "PP");
    DrawBar(490, 48, 68, 8, c->pp, c->maxPp, kPpBlue);
    Text(sFont12, 564, 42, 0, C_WHITE, "%d/%d", c->pp, c->maxPp);
    AddHit(446, 36, 186, 30, H_PP);
    /* stats */
    Window(192, 88, 140, 292);
    Chip(200, 81, "STATS");
    static const char* kStat[6] = { "Attack", "Defense", "Agility", "Luck", "Exp", "Coins" };
    const unsigned val[6] = { (unsigned)c->attack, (unsigned)c->defense, (unsigned)c->agility, (unsigned)c->luck, c->exp, sSt.coins };
    for (int i = 0; i < 6; i++) {
        const int y = 100 + i * 46;
        Text(sFont14, 206, y + 8, 0, kLbl, "%s", kStat[i]);
        Text(sFont18, 318, y + 6, 2, i == 5 ? kCoin : C_WHITE, "%u", val[i]);
        if (i < 5) FillRect(204, y + 38, 116, 1, kPanelDk);
    }
    AddHit(196, 100 + 5 * 46, 132, 42, H_COINS);
    /* the Djinn each element has set, and the power / resist they give */
    Window(340, 88, 292, 292);
    Chip(348, 81, "ELEMENTS");
    for (int e = 0; e < ELEMENT_COUNT; e++) {
        const int y = 100 + e * 66;
        FillRect(352, y, 268, 58, kPanelDk);
        FillRect(352, y, 6, 58, kElementCol[e]);
        Text(sFont14, 368, y + 6, 0, C_WHITE, "%s", kElementName[e]);
        Text(sFont12, 612, y + 8, 2, kLbl, "Djinn %d / %d", c->setCount[e], c->ownedCount[e]);
        Text(sFont12, 368, y + 34, 0, kLbl, "Power");
        Text(sFont18, 440, y + 30, 0, C_WHITE, "%d", c->power[e]);
        Text(sFont12, 488, y + 34, 0, kLbl, "Resist");
        Text(sFont18, 560, y + 30, 0, C_WHITE, "%d", c->resist[e]);
    }
    (void)ec;
}

static void DrawPsynergy(void) {
    DrawPartyList(true);
    const Char* c = &sSt.ch[sMember];
    /* Psynergy the pause menu can cast; tapping one drives the game's own menu */
    Window(192, 6, 440, 374);
    Chip(200, 0, "PSYNERGY");
    Text(sFont18, 208, 16, 0, C_WHITE, "%s", kCharName[sMember]);
    Text(sFont14, 622, 20, 2, kLbl, "PP %d / %d", c->pp, c->maxPp);
    if (sPsySel >= 0) {
        const PsyInfo* pi = PsyLookup(sPsy[sMember][sPsySel].id);
        Text(sFont18, 208, 52, 0, C_WHITE, "%s on whom?", pi->name);
        for (int k = 0; k < sSt.orderCount; k++) {
            const int i = sSt.order[k];
            const Char* t = &sSt.ch[i];
            const int y = 86 + k * 62;
            FillRect(208, y, 408, 54, kPink);
            Text(sFont22, 224, y + 13, 0, C_WHITE, "%s", kCharName[i]);
            Text(sFont14, 604, y + 18, 2, C_WHITE, "HP %d/%d", t->hp, t->maxHp);
            AddHit(208, y, 408, 54, H_TARGET + i);
        }
        FillRect(208, 336, 408, 36, kPanelDk);
        FrameRect(208, 336, 408, 36, C_WHITE);
        Text(sFont14, 412, 345, 1, C_WHITE, "Back");
        AddHit(208, 336, 408, 36, H_TARGET_BACK);
    } else if (!sPsyScanned) {
        Text(sFont14, 412, 140, 1, kLbl, "Psynergy is read from the game's pause menu.");
        FillRect(300, 190, 224, 56, kPink);
        FrameRect(300, 190, 224, 56, C_WHITE);
        Text(sFont18, 412, 206, 1, C_WHITE, "Read Psynergy");
        AddHit(300, 190, 224, 56, H_PSY_SCAN);
    } else {
        const int n = sPsyCount[sMember];
        const int rh = n > 9 ? 304 / n - 2 : 32;
        for (int i = 0; i < n; i++) {
            const PsyInfo* pi = PsyLookup(sPsy[sMember][i].id);
            const int y = 46 + i * (rh + 2);
            const bool ok = pi->cost == 0 || c->pp >= pi->cost;
            FillRect(204, y, 420, rh, ok ? kPanelDk : kTrack);
            if (ok) FillRect(204, y, 4, rh, pi->kind == PSY_FIELD ? kLbl : kHpGreen);
            Text(sFont18, 222, y + (rh - 22) / 2, 0, ok ? C_WHITE : C_GREY, "%s", pi->name);
            if (pi->cost) Text(sFont14, 610, y + (rh - 17) / 2, 2, ok ? kLbl : C_GREY, "%d PP", pi->cost);
            if (ok) AddHit(204, y, 420, rh, H_PSY + i);
        }
        if (n == 0) Text(sFont14, 412, 190, 1, kLbl, "No Psynergy for the pause menu");
        FillRect(544, 350, 76, 22, kPanelDk);
        FrameRect(544, 350, 76, 22, kLbl);
        Text(sFont12, 582, 354, 1, kLbl, "Re-read");
        AddHit(534, 344, 92, 34, H_PSY_SCAN);
    }
}

static void DrawDjinn(void) {
    Window(8, 6, W - 16, 374);
    const int colW = 150, rowH = 19;
    for (int e = 0; e < ELEMENT_COUNT; e++) {
        const int x = 18 + e * (colW + 2);
        FillRect(x, 14, colW - 6, 22, kElementCol[e]);
        Text(sFont14, x + (colW - 6) / 2, 17, 1, C_WHITE, "%s", kElementName[e]);
        for (int d = 0; d < DJINN_PER_ELEMENT; d++) {
            const int y = 40 + d * rowH;
            int owner = -1;
            bool set = false;
            for (int i = 0; i < PARTY_COUNT; i++)
                if ((sSt.ch[i].djinnOwned[e] >> d) & 1) {
                    owner = i;
                    set = (sSt.ch[i].djinnSet[e] >> d) & 1;
                    break;
                }
            FillRect(x, y, colW - 6, rowH - 2, owner >= 0 ? kPanelDk : kTrack);
            if (owner >= 0 && set) FillRect(x, y, 3, rowH - 2, kElementCol[e]);
            Text(sFont12, x + 8, y + 1, 0, owner >= 0 ? C_WHITE : C_GREY, "%s", kDjinnName[e][d]);
            if (owner >= 0) Text(sFont12, x + colW - 12, y + 1, 2, set ? kHpGreen : kCoin, "%.3s%s", kCharName[owner], set ? "" : "*");
        }
    }
}

static void DrawNote(const char* l1, const char* l2) {
    Text(sFont22, W / 2, 180, 1, C_WHITE, "THE LOST AGE");
    Text(sFont18, W / 2, 214, 1, C_WHITE, "%s", l1);
    Text(sFont14, W / 2, 246, 1, kLbl, "%s", l2);
}

static void DrawFrame(void) {
    SDL_SetRenderDrawColor(sRen, kBg.r, kBg.g, kBg.b, 255);
    SDL_RenderClear(sRen);
    sHitCount = 0;
    if (!sConnected) {
        DrawNote("Waiting for RetroArch...", "Launch Golden Sun (mGBA core)");
    } else if (!sSt.valid) {
        DrawNote("Load a saved game", "Choose Continue on the title screen to begin");
    } else {
        if (!sSt.ch[sMember].joined || (sTab == TAB_PSY && !sSt.ch[sMember].inParty)) sMember = sSt.order[0];
        switch (sTab) {
            case TAB_PARTY: DrawParty(); break;
            case TAB_DJINN: DrawDjinn(); break;
            case TAB_PSY: DrawPsynergy(); break;
        }
        DrawTabs();
    }
    if (sBusy) {
        FillRect(0, 0, W, H, kBg);
        Window(110, 180, 420, 120);
        Text(sFont22, W / 2, 214, 1, C_WHITE, "%s", sBusyMsg);
        Text(sFont12, W / 2, 256, 1, kLbl, "driving the game's menu on the top screen");
    }
    if (SDL_GetTicks() < sToastUntil) {
        const int tw = TextWidth(sFont18, sToast) + 28;
        FillRect(W / 2 - tw / 2, 340, tw, 28, kPink);
        FrameRect(W / 2 - tw / 2, 340, tw, 28, C_WHITE);
        Text(sFont18, W / 2, 343, 1, C_WHITE, "%s", sToast);
    }
    SDL_RenderPresent(sRen);
}

static void OnTap(int x, int y) {
    const int id = HitTest(x, y);
    if (id < 0)
        return;
    if (sBusy) return;
    if (id >= H_TAB && id < H_TAB + TAB_COUNT) {
        sTab = id - H_TAB;
        sPsySel = -1;
    } else if (id >= H_CHAR && id < H_CHAR + PARTY_COUNT) {
        sMember = id - H_CHAR;
        sPsySel = -1;
    } else if (id == H_PSY_SCAN) {
        ScanPsy();
    } else if (id >= H_PSY && id < H_PSY + PSY_MAX && sTab == TAB_PSY) {
        const int i = id - H_PSY;
        if (i < sPsyCount[sMember]) {
            if (PsyLookup(sPsy[sMember][i].id)->kind == PSY_ONE) sPsySel = i;
            else CastPsy(sMember, i, 0);
        }
    } else if (id == H_TARGET_BACK) {
        sPsySel = -1;
    } else if (id >= H_TARGET && id < H_TARGET + PARTY_COUNT && sPsySel >= 0) {
        const int i = sPsySel;
        sPsySel = -1;
        CastPsy(sMember, i, id - H_TARGET);
    } else if (id == H_COINS) {
        WriteU32(RAM_COINS, sSt.coins + 10000u > 999999u ? 999999u : sSt.coins + 10000u);
        Toast("+10,000 coins");
    } else if (id == H_HP) {
        WriteU16(CharAddr(sMember) + CH_HP, sSt.ch[sMember].maxHp);
        Toast("%s: HP full", kCharName[sMember]);
    } else if (id == H_PP) {
        WriteU16(CharAddr(sMember) + CH_PP, sSt.ch[sMember].maxPp);
        Toast("%s: PP full", kCharName[sMember]);
    } else if (id == H_HEAL_ALL) {
        for (int k = 0; k < sSt.orderCount; k++) {
            const int i = sSt.order[k];
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
    fprintf(stderr, "[gsl] tap (%d,%d) -> %d\n", x, y, id);
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
    if (getenv("GSL_DATA_DIR"))
        snprintf(dir, sizeof dir, "%s", getenv("GSL_DATA_DIR"));
    if (!RaClient_Open("127.0.0.1", 55355)) {
        fprintf(stderr, "[gsl] cannot open the RetroArch command socket\n");
        return 1;
    }
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0 || TTF_Init() != 0) {
        fprintf(stderr, "[gsl] SDL init failed: %s\n", SDL_GetError());
        return 1;
    }
    const bool windowed = getenv("GSL_WINDOWED") != NULL || SDL_GetNumVideoDisplays() < 2;
    Uint32 flags = SDL_WINDOW_SHOWN;
    int px = SDL_WINDOWPOS_UNDEFINED, py = SDL_WINDOWPOS_UNDEFINED;
    if (!windowed) {
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_BORDERLESS;
        px = SDL_WINDOWPOS_UNDEFINED_DISPLAY(1);
        py = SDL_WINDOWPOS_UNDEFINED_DISPLAY(1);
    }
    /* the sway config moves windows titled "...Bottom Screen" to DSI-1 */
    sWin = SDL_CreateWindow("Golden Sun The Lost Age - Bottom Screen", px, py, W, H, flags);
    if (!sWin) {
        fprintf(stderr, "[gsl] window failed: %s\n", SDL_GetError());
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
        fprintf(stderr, "[gsl] cannot load font %s: %s\n", fontPath, TTF_GetError());
        return 1;
    }
    sTouchFd = OpenBottomTouch();

    /* QA: GSL_TEST_TAPS="ms,x,y;ms,x,y" injects taps through the real tap handler */
    struct { unsigned ms; int x, y; bool done; } taps[32];
    int tapCount = 0;
    if (getenv("GSL_TEST_TAPS")) {
        char* spec = strdup(getenv("GSL_TEST_TAPS"));
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
                fprintf(stderr, "[gsl] test tap (%d,%d)\n", taps[i].x, taps[i].y);
                OnTap(taps[i].x, taps[i].y);
            }
        const Uint32 now = SDL_GetTicks();
        if (now - lastPoll >= 150) {
            lastPoll = now;
            if (PollState()) {
                if (!sConnected)
                    fprintf(stderr, "[gsl] connected to RetroArch\n");
                sConnected = true;
                silent = 0;
            } else if (++silent > 7u * (sConnected ? MAX_SILENT_SECONDS : STARTUP_GRACE_SECONDS)) {
                fprintf(stderr, "[gsl] RetroArch is gone, exiting\n");
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
