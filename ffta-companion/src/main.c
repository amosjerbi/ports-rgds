/* Final Fantasy Tactics Advance (USA) companion for the Anystream RG DS: runs on the bottom panel next to RetroArch's mGBA core on the top panel.
 * Reads and writes the game's RAM over RetroArch's network command interface (UDP 55355).
 * Tabs: PARTY (the clan's units with job, HP / MP, base stats and equipment; tap a bar to refill it),
 *       ITEMS (the shared inventory with owned / equipped counts), CLAN (funds, clan level and the full roster). */
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ffta_data.h"
#include "ra_client.h"

#define W 640
#define H 480
#define MAX_SILENT_SECONDS 20
#define STARTUP_GRACE_SECONDS 180

#include "ui_common.h"

/* ------------------------------------------------------------------------------------------------------------------ */
/* game state                                                                                                         */
/* ------------------------------------------------------------------------------------------------------------------ */
typedef struct {
    char name[20];
    uint32_t namePtr;
    int level, job, hp, maxHp, mp, maxMp, speed;
    int stat[4];
    int equip[EQUIP_SLOTS];
} Unit;
typedef struct { int id, owned, equipped; } InvItem;
typedef struct {
    bool valid;
    Unit u[UNIT_COUNT];
    int unitCount; /* units are packed from index 0 */
    uint32_t gil;
    int clanLevel, clanPoints;
    char clanName[20];
    InvItem inv[INV_ENTRIES];
    int invCount;
} State;
static State sSt;
static bool sConnected, sRomOk, sNamesOk;
static char sItemName[ITEM_ID_LAST + 1][28];

static int U16(const uint8_t* b, int o) { return b[o] | (b[o + 1] << 8); }
static uint32_t U32(const uint8_t* b, int o) { return (uint32_t)(b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | ((uint32_t)b[o + 3] << 24)); }

/* The game's text: letters are one byte (A = 0xB1, a = 0xCB) or, for typed names, 0x80 followed by a letter (A = 0xB0, a = 0xCA); some of the
 * cartridge's own words use 0x80 followed by the ASCII letter minus one. 0x40 0x73 is a space, 0x40 0x3E / 0x40 0x3C carry a layout byte. */
static void DecodeText(const uint8_t* b, int n, char* out, int cap) {
    int i = 0, o = 0;
    if (n > 0 && b[0] == 1) i = 1;
    while (i < n && o < cap - 1) {
        int c = b[i++], ch = 0;
        if (c == 0) break;
        if (c == 0x80 && i < n) {
            const int c2 = b[i++];
            if (c2 >= 0xB0 && c2 <= 0xC9) ch = 'A' + c2 - 0xB0;
            else if (c2 >= 0xCA && c2 <= 0xE3) ch = 'a' + c2 - 0xCA;
            else if (c2 >= 0x20 && c2 < 0x7F) ch = c2 + 1;
        } else if (c >= 0xB1 && c <= 0xCA) ch = 'A' + c - 0xB1;
        else if (c >= 0xCB && c <= 0xE4) ch = 'a' + c - 0xCB;
        else if (c == 0x40 && i < n) {
            const int c2 = b[i++];
            if (c2 == 0x73) ch = ' ';
            else if ((c2 == 0x3E || c2 == 0x3C) && i < n) {
                i++;
                if (c2 == 0x3E) ch = ' ';
            }
        } else if (c == 0xF0 || c == 0xFF) ch = '-';
        else if (c == 0xA9 || c == 0xAD || c == 0xAE) ch = '\'';
        if (ch) out[o++] = (char)ch;
    }
    out[o] = 0;
}

/* names of the items (table index = id + 0x7B), read once from the cartridge */
static bool LoadItemNames(void) {
    static uint8_t pool[ROM_POOL_BYTES], tab[ITEM_ID_LAST * 4];
    if (!RaClient_Read(ROM_POOL, pool, sizeof pool) || !RaClient_Read(ROM_NAME_TABLE + 4 * (ITEM_INDEX_BASE + 1), tab, sizeof tab))
        return false;
    for (int id = 1; id <= ITEM_ID_LAST; id++) {
        const uint32_t p = U32(tab, 4 * (id - 1));
        if (p >= ROM_POOL && p < ROM_POOL + ROM_POOL_BYTES - 4)
            DecodeText(pool + (p - ROM_POOL), (int)(ROM_POOL + ROM_POOL_BYTES - p), sItemName[id], sizeof sItemName[id]);
        if (!sItemName[id][0]) snprintf(sItemName[id], sizeof sItemName[id], "Item %d", id);
    }
    return true;
}

typedef struct { int id; const char* name; } JobInfo;
static const JobInfo kJob[] = {
    { 2, "Soldier" }, { 3, "Paladin" }, { 5, "Dragoon" }, { 6, "Templar" }, { 9, "White Mage" }, { 10, "Black Mage" },
    { 11, "Blue Mage" }, { 14, "Summoner" }, { 16, "Ninja" }, { 18, "Sage" }, { 24, "Thief" }, { 25, "Assassin" },
    { 26, "Sniper" }, { 27, "Archer" }, { 32, "Mog Knight" },
};
static const char* JobName(int id) {
    static char nm[16];
    for (size_t i = 0; i < sizeof kJob / sizeof kJob[0]; i++)
        if (kJob[i].id == id) return kJob[i].name;
    snprintf(nm, sizeof nm, "Job %d", id);
    return nm;
}

static bool PollState(void) {
    static uint8_t blk[UNIT_COUNT * UNIT_STRIDE];
    static Uint32 lastNames, lastInv;
    uint8_t g[4], lv[2], cn[20];
    if (!RaClient_Read(RAM_UNITS, blk, sizeof blk))
        return false;
    if (!sRomOk) {
        uint8_t code[4] = { 0 };
        if (RaClient_Read(FFTA_ROM_CODE_ADDR, code, 4) && memcmp(code, "AFXE", 4) == 0) {
            sRomOk = true;
            fprintf(stderr, "[ffta] Final Fantasy Tactics Advance (USA) detected\n");
        } else if (code[0] && memcmp(code, "AFXE", 4) != 0) {
            fprintf(stderr, "[ffta] not Final Fantasy Tactics Advance (USA): %.4s\n", code);
        }
    }
    if (sRomOk && !sNamesOk) sNamesOk = LoadItemNames();
    const Uint32 now = SDL_GetTicks();
    const bool refreshNames = now - lastNames > 3000;
    if (refreshNames) lastNames = now;
    int n = 0;
    for (int i = 0; i < UNIT_COUNT; i++) {
        const uint8_t* b = blk + i * UNIT_STRIDE;
        Unit* u = &sSt.u[i];
        u->level = b[U_LEVEL];
        u->hp = U16(b, U_HP);
        u->maxHp = U16(b, U_HP + 2);
        u->mp = U16(b, U_MP);
        u->maxMp = U16(b, U_MP + 2);
        u->job = b[U_JOB];
        u->speed = U16(b, U_SPEED);
        for (int k = 0; k < 4; k++) u->stat[k] = U16(b, U_STATS + 2 * k);
        for (int k = 0; k < EQUIP_SLOTS; k++) u->equip[k] = U16(b, U_EQUIP + 2 * k);
        const uint32_t p = U32(b, U_NAME_PTR);
        if (u->level >= 1 && u->level <= 99 && u->maxHp > 0 && (p >> 24 == 2 || p >> 24 == 8)) {
            if (p != u->namePtr || refreshNames || !u->name[0]) {
                uint8_t t[24] = { 0 };
                if (RaClient_Read(p, t, sizeof t)) {
                    DecodeText(t, sizeof t, u->name, sizeof u->name);
                    u->namePtr = p;
                }
            }
            n = i + 1;
        } else {
            u->level = 0;
        }
    }
    sSt.unitCount = n;
    sSt.valid = sRomOk && n >= 1 && sSt.u[0].level > 0;
    if (RaClient_Read(RAM_GIL, g, 4)) sSt.gil = U32(g, 0);
    if (RaClient_Read(RAM_CLAN_LEVEL, lv, 2)) {
        sSt.clanLevel = lv[0];
        sSt.clanPoints = lv[1];
    }
    if (refreshNames && RaClient_Read(RAM_CLAN_NAME, cn, sizeof cn)) DecodeText(cn, sizeof cn, sSt.clanName, sizeof sSt.clanName);
    if (now - lastInv > 400 || sSt.invCount == 0) {
        static uint8_t inv[INV_BYTES];
        if (RaClient_Read(RAM_INV, inv, sizeof inv)) {
            lastInv = now;
            int c = 0;
            for (int o = 0; o + 4 <= INV_BYTES; o += 4) {
                const int id = U16(inv, o);
                if (id >= 1 && id <= ITEM_ID_LAST && inv[o + 2] > 0) {
                    int k;
                    for (k = 0; k < c; k++)
                        if (sSt.inv[k].id == id) break;
                    if (k == c) sSt.inv[c++] = (InvItem){ id, 0, 0 };
                    sSt.inv[k].owned += inv[o + 2];
                    sSt.inv[k].equipped += inv[o + 3];
                }
            }
            for (int a = 1; a < c; a++) { /* by id, which groups weapons, armor and consumables like the game */
                const InvItem t = sSt.inv[a];
                int b = a - 1;
                while (b >= 0 && sSt.inv[b].id > t.id) { sSt.inv[b + 1] = sSt.inv[b]; b--; }
                sSt.inv[b + 1] = t;
            }
            sSt.invCount = c;
        }
    }
    return true;
}

static void WriteU16(uint32_t addr, int v) {
    const uint8_t b[2] = { (uint8_t)(v & 255), (uint8_t)(v >> 8) };
    RaClient_Write(addr, b, 2);
}
static uint32_t UnitAddr(int i) { return RAM_UNITS + (uint32_t)i * UNIT_STRIDE; }

/* ------------------------------------------------------------------------------------------------------------------ */
/* UI: the same blue menu look as the Golden Sun companions                                                             */
/* ------------------------------------------------------------------------------------------------------------------ */
enum { TAB_PARTY, TAB_ITEMS, TAB_CLAN, TAB_COUNT };
static const char* kTabName[TAB_COUNT] = { "Party", "Items", "Clan" };
enum {
    H_TAB = 100,
    H_UNIT = 110, /* + unit (0..19) */
    H_HP = 140,
    H_MP = 141,
    H_LIST_UP = 142,
    H_LIST_DOWN = 143,
    H_FILTER = 150, /* + filter */
    H_ROSTER = 170  /* + unit */
};
enum { F_ALL, F_WEAPON, F_ARMOR, F_USE, F_COUNT };
static const char* kFilterName[F_COUNT] = { "All", "Weapons", "Armor", "Items" };
static int sTab = TAB_PARTY, sMember, sFilter, sUnitScroll, sItemScroll; /* scrolls count rows */
static char sToast[96];
static Uint32 sToastUntil;

static void Toast(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(sToast, sizeof sToast, fmt, ap);
    va_end(ap);
    sToastUntil = SDL_GetTicks() + 2200;
}

static const Col C_GREY = { 120, 140, 200 };
static const Col kBg = { 4, 14, 112 }, kPanel = { 12, 38, 158 }, kPanelDk = { 6, 24, 112 }, kEdge = { 0, 0, 48 },
                 kPink = { 226, 100, 172 }, kPinkDk = { 150, 44, 108 }, kLbl = { 150, 176, 236 }, kCoin = { 255, 205, 70 },
                 kHpGreen = { 90, 214, 60 }, kPpBlue = { 40, 160, 240 }, kTrack = { 8, 18, 76 }, kOrange = { 255, 150, 30 };
static const Col kJobCol[] = { { 224, 150, 50 }, { 70, 140, 230 }, { 214, 70, 60 }, { 160, 110, 220 }, { 90, 200, 110 }, { 230, 200, 80 } };

static void DrawBar(int x, int y, int w, int h, int cur, int max, Col c) {
    FillRect(x, y, w, h, kTrack);
    if (max > 0)
        FillRect(x, y, (int)((long)w * (cur > max ? max : cur) / max), h, c);
    FrameRect(x, y, w, h, kEdge);
}

static void Window(int x, int y, int w, int h) {
    FillRect(x, y, w, h, kEdge);
    FillRect(x + 1, y + 1, w - 2, h - 2, C_WHITE);
    FillRect(x + 3, y + 3, w - 6, h - 6, kPanel);
}

static void Chip(int x, int y, const char* label) {
    const int w = TextWidth(sFont12, label) + 14;
    FillRect(x, y, w, 15, kPinkDk);
    FillRect(x + 1, y + 1, w - 2, 13, kEdge);
    Text(sFont12, x + w / 2, y + 1, 1, C_WHITE, "%s", label);
}

static void SetClip(int x, int y, int w, int h) {
    const SDL_Rect r = { x, y, w, h };
    SDL_RenderSetClipRect(sRen, &r);
}
static void ClearClip(void) { SDL_RenderSetClipRect(sRen, NULL); }

/* adds a hit area only for the part of a row that is inside the visible list */
static void AddHitIn(int x, int y, int w, int h, int id, int top, int bottom) {
    const int y0 = y < top ? top : y, y1 = y + h > bottom ? bottom : y + h;
    if (y1 - y0 >= 14) AddHit(x, y0, w, y1 - y0, id);
}

static int Clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static void DrawTabs(void) {
    Window(8, 388, W - 16, 86);
    const int tw = (W - 28) / TAB_COUNT;
    for (int t = 0; t < TAB_COUNT; t++) {
        const int x = 14 + t * tw, y = 394, h = 74;
        if (t == sTab) FillRect(x, y, tw, h, kPink);
        else if (t > 0) FillRect(x + 0, y + 6, 2, h - 12, kPanelDk);
        Text(sFont18, x + tw / 2, y + h / 2 - 12, 1, C_WHITE, "%s", kTabName[t]);
        AddHit(x, y, tw, h, H_TAB + t);
    }
}

/* a thin scroll strip: tap the upper half to page up, the lower half to page down */
static void DrawScrollStrip(int x, int y, int h, int pos, int total, int visible) {
    FillRect(x, y, 16, h, kPanelDk);
    if (total > visible) {
        const int th = h * visible / total > 24 ? h * visible / total : 24;
        const int ty = y + (h - th) * pos / (total - visible);
        FillRect(x + 2, ty, 12, th, kPink);
    }
    Text(sFont12, x + 8, y + 2, 1, C_WHITE, "^");
    Text(sFont12, x + 8, y + h - 16, 1, C_WHITE, "v");
    AddHit(x, y, 16, h / 2, H_LIST_UP);
    AddHit(x, y + h / 2, 16, h - h / 2, H_LIST_DOWN);
}

#define UROW_H 44
#define UROWS 8
static void DrawUnitList(void) {
    Window(8, 6, 176, 374);
    const int maxScroll = sSt.unitCount > UROWS ? sSt.unitCount - UROWS : 0;
    sUnitScroll = Clamp(sUnitScroll, 0, maxScroll);
    SetClip(12, 12, 140, UROWS * UROW_H + 4);
    for (int i = 0; i < sSt.unitCount; i++) {
        const Unit* u = &sSt.u[i];
        const int y = 14 + (i - sUnitScroll) * UROW_H;
        if (y < 0 || y > 14 + UROWS * UROW_H) continue;
        FillRect(13, y, 140, UROW_H - 3, i == sMember ? kPink : kPanelDk);
        Text(sFont14, 20, y + 3, 0, C_WHITE, "%s", u->name);
        Text(sFont12, 148, y + 5, 2, i == sMember ? C_WHITE : kLbl, "L%d", u->level);
        Text(sFont12, 20, y + 20, 0, i == sMember ? C_WHITE : kLbl, "%s", JobName(u->job));
        DrawBar(20, y + 35, 126, 4, u->hp, u->maxHp, u->hp * 4 < u->maxHp ? kOrange : kHpGreen);
        AddHitIn(13, y, 140, UROW_H - 3, H_UNIT + i, 12, 12 + UROWS * UROW_H);
    }
    ClearClip();
    DrawScrollStrip(158, 12, UROWS * UROW_H + 4, sUnitScroll, sSt.unitCount, UROWS);
}

static int EquipKind(int id) {
    if (id < ITEM_ID_ARMOR) return 0;
    if (id < ITEM_ID_BODY) return 1;
    if (id < ITEM_ID_FEET) return 2;
    return id < ITEM_ID_ACC ? 3 : 4;
}

static void DrawParty(void) {
    DrawUnitList();
    const Unit* u = &sSt.u[sMember];
    Window(192, 6, 440, 66);
    const Col jc = kJobCol[u->job % 6];
    FillRect(204, 14, 48, 50, kEdge);
    FillRect(206, 16, 44, 46, kPanelDk);
    Text(sFont22, 228, 26, 1, jc, "%c", u->name[0] ? u->name[0] : '?');
    Text(sFont18, 264, 14, 0, C_WHITE, "%s", u->name);
    const int nw = TextWidth(sFont18, u->name);
    Text(sFont14, 264 + nw + 10, 18, 0, C_WHITE, "L%d", u->level);
    Text(sFont12, 264 + nw + 52, 20, 0, kLbl, "%s", JobName(u->job));
    Text(sFont12, 264, 44, 0, kPink, "HP");
    DrawBar(286, 48, 112, 8, u->hp, u->maxHp, u->hp * 4 < u->maxHp ? kOrange : kHpGreen);
    Text(sFont12, 402, 42, 0, C_WHITE, "%d/%d", u->hp, u->maxHp);
    AddHit(258, 36, 190, 30, H_HP);
    Text(sFont12, 468, 44, 0, kPink, "MP");
    DrawBar(490, 48, 68, 8, u->mp, u->maxMp, kPpBlue);
    Text(sFont12, 564, 42, 0, C_WHITE, "%d/%d", u->mp, u->maxMp);
    AddHit(446, 36, 186, 30, H_MP);
    /* base stats */
    Window(192, 88, 140, 292);
    Chip(200, 81, "STATS");
    static const char* kStat[5] = { "Attack", "Defense", "Magic Pow", "Magic Res", "Speed" };
    const int val[5] = { u->stat[0], u->stat[1], u->stat[2], u->stat[3], u->speed };
    for (int i = 0; i < 5; i++) {
        const int y = 100 + i * 46;
        Text(sFont14, 206, y + 8, 0, kLbl, "%s", kStat[i]);
        Text(sFont18, 318, y + 6, 2, C_WHITE, "%d", val[i]);
        FillRect(204, y + 38, 116, 1, kPanelDk);
    }
    Text(sFont12, 262, 340, 1, C_GREY, "base, before gear");
    /* what the unit wears */
    Window(340, 88, 292, 292);
    Chip(348, 81, "EQUIPMENT");
    static const char* kSlot[EQUIP_SLOTS] = { "Weapon", "Head", "Body", "Feet", "Accessory" };
    int worn[EQUIP_SLOTS] = { 0 }; /* the game lists them in a per-job order; show them by kind */
    for (int k = 0; k < EQUIP_SLOTS; k++)
        if (u->equip[k]) worn[EquipKind(u->equip[k])] = u->equip[k];
    for (int s = 0; s < EQUIP_SLOTS; s++) {
        const int y = 100 + s * 55;
        const int id = worn[s];
        FillRect(352, y, 268, 49, kPanelDk);
        Text(sFont12, 364, y + 5, 0, kLbl, "%s", kSlot[s]);
        if (id >= 1 && id <= ITEM_ID_LAST) Text(sFont18, 364, y + 21, 0, C_WHITE, "%s", sItemName[id]);
        else Text(sFont18, 364, y + 21, 0, C_GREY, "None");
    }
}

static bool Matches(int id, int f) {
    if (f == F_ALL) return true;
    if (f == F_WEAPON) return id < ITEM_ID_ARMOR;
    if (f == F_ARMOR) return id >= ITEM_ID_ARMOR && id < ITEM_ID_USE;
    return id >= ITEM_ID_USE;
}

#define IROW_H 36
#define IROWS 8
static void DrawItems(void) {
    Window(8, 6, W - 16, 374);
    const int fw = (W - 40) / F_COUNT;
    for (int f = 0; f < F_COUNT; f++) {
        const int x = 16 + f * fw;
        FillRect(x, 14, fw - 6, 34, f == sFilter ? kPink : kPanelDk);
        Text(sFont18, x + (fw - 6) / 2, 19, 1, C_WHITE, "%s", kFilterName[f]);
        AddHit(x, 14, fw - 6, 34, H_FILTER + f);
    }
    int idx[INV_ENTRIES], n = 0, total = 0;
    for (int i = 0; i < sSt.invCount; i++)
        if (Matches(sSt.inv[i].id, sFilter)) {
            idx[n++] = i;
            total += sSt.inv[i].owned;
        }
    const int rows = (n + 1) / 2;
    sItemScroll = Clamp(sItemScroll, 0, rows > IROWS ? rows - IROWS : 0);
    SetClip(14, 54, 580, IROWS * IROW_H);
    const int colW = 286;
    for (int k = 0; k < n; k++) {
        const int r = k / 2 - sItemScroll, c = k % 2;
        if (r < 0 || r >= IROWS) continue;
        const InvItem* it = &sSt.inv[idx[k]];
        const int x = 16 + c * (colW + 6), y = 54 + r * IROW_H;
        FillRect(x, y, colW, IROW_H - 3, kPanelDk);
        Text(sFont14, x + 10, y + 8, 0, C_WHITE, "%s", sItemName[it->id]);
        if (it->equipped) Text(sFont12, x + colW - 54, y + 11, 2, kCoin, "%d worn", it->equipped);
        Text(sFont18, x + colW - 8, y + 5, 2, kLbl, "%d", it->owned);
    }
    ClearClip();
    if (n == 0) Text(sFont14, 300, 190, 1, kLbl, "Nothing here");
    DrawScrollStrip(600, 54, IROWS * IROW_H, sItemScroll, rows, IROWS);
    Text(sFont12, 16, 346, 0, kLbl, "%d kinds, %d in all", n, total);
}

static void DrawClan(void) {
    Window(8, 6, 250, 374);
    Chip(16, 0, "CLAN");
    Text(sFont22, 22, 20, 0, C_WHITE, "%s", sSt.clanName[0] ? sSt.clanName : "Clan");
    FillRect(20, 54, 226, 1, kPanelDk);
    Text(sFont14, 22, 64, 0, kLbl, "Clan funds");
    Text(sFont22, 244, 86, 2, kCoin, "%u gil", sSt.gil);
    FillRect(20, 124, 226, 1, kPanelDk);
    Text(sFont14, 22, 134, 0, kLbl, "Clan level");
    Text(sFont18, 244, 132, 2, C_WHITE, "%d", sSt.clanLevel);
    Text(sFont14, 22, 168, 0, kLbl, "Clan points");
    Text(sFont18, 244, 166, 2, C_WHITE, "%d", sSt.clanPoints);
    int total = 0;
    for (int i = 0; i < sSt.invCount; i++) total += sSt.inv[i].owned;
    Text(sFont14, 22, 202, 0, kLbl, "Members");
    Text(sFont18, 244, 200, 2, C_WHITE, "%d", sSt.unitCount);
    Text(sFont14, 22, 236, 0, kLbl, "Item kinds");
    Text(sFont18, 244, 234, 2, C_WHITE, "%d", sSt.invCount);
    Text(sFont14, 22, 270, 0, kLbl, "Items in all");
    Text(sFont18, 244, 268, 2, C_WHITE, "%d", total);
    /* the roster: tap a member to open them on the Party tab */
    Window(266, 6, 366, 374);
    Chip(274, 0, "ROSTER");
    for (int i = 0; i < sSt.unitCount && i < 20; i++) {
        const Unit* u = &sSt.u[i];
        const int c = i / 10, r = i % 10;
        const int x = 274 + c * 178, y = 18 + r * 35;
        FillRect(x, y, 172, 32, kPanelDk);
        Text(sFont14, x + 8, y + 2, 0, C_WHITE, "%s", u->name);
        Text(sFont12, x + 166, y + 4, 2, kLbl, "L%d", u->level);
        Text(sFont12, x + 8, y + 18, 0, kLbl, "%s", JobName(u->job));
        AddHit(x, y, 172, 32, H_ROSTER + i);
    }
}

static void DrawNote(const char* l1, const char* l2) {
    Text(sFont22, W / 2, 160, 1, C_WHITE, "FINAL FANTASY TACTICS ADVANCE");
    Text(sFont18, W / 2, 214, 1, C_WHITE, "%s", l1);
    Text(sFont14, W / 2, 246, 1, kLbl, "%s", l2);
}

static void DrawFrame(void) {
    SDL_SetRenderDrawColor(sRen, kBg.r, kBg.g, kBg.b, 255);
    SDL_RenderClear(sRen);
    sHitCount = 0;
    if (!sConnected) {
        DrawNote("Waiting for RetroArch...", "Launch Final Fantasy Tactics Advance (mGBA core)");
    } else if (!sSt.valid) {
        DrawNote("Load a saved game", "Choose Load on the title screen to begin");
    } else {
        if (sMember >= sSt.unitCount) sMember = 0;
        switch (sTab) {
            case TAB_PARTY: DrawParty(); break;
            case TAB_ITEMS: DrawItems(); break;
            case TAB_CLAN: DrawClan(); break;
        }
        DrawTabs();
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
    if (id >= H_TAB && id < H_TAB + TAB_COUNT) {
        sTab = id - H_TAB;
    } else if (id >= H_UNIT && id < H_UNIT + UNIT_COUNT) {
        sMember = id - H_UNIT;
    } else if (id >= H_ROSTER && id < H_ROSTER + UNIT_COUNT) {
        sMember = id - H_ROSTER;
        sTab = TAB_PARTY;
        sUnitScroll = Clamp(sMember - UROWS / 2, 0, sSt.unitCount > UROWS ? sSt.unitCount - UROWS : 0);
    } else if (id >= H_FILTER && id < H_FILTER + F_COUNT) {
        sFilter = id - H_FILTER;
        sItemScroll = 0;
    } else if (id == H_LIST_UP || id == H_LIST_DOWN) {
        const int d = id == H_LIST_UP ? -1 : 1;
        if (sTab == TAB_PARTY) sUnitScroll += d * (UROWS - 1);
        else sItemScroll += d * (IROWS - 1);
    } else if (id == H_HP) {
        WriteU16(UnitAddr(sMember) + U_HP, sSt.u[sMember].maxHp);
        Toast("%s: HP full", sSt.u[sMember].name);
    } else if (id == H_MP) {
        WriteU16(UnitAddr(sMember) + U_MP, sSt.u[sMember].maxMp);
        Toast("%s: MP full", sSt.u[sMember].name);
    }
    fprintf(stderr, "[ffta] tap (%d,%d) -> %d\n", x, y, id);
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
    if (getenv("FFTA_DATA_DIR"))
        snprintf(dir, sizeof dir, "%s", getenv("FFTA_DATA_DIR"));
    if (!RaClient_Open("127.0.0.1", 55355)) {
        fprintf(stderr, "[ffta] cannot open the RetroArch command socket\n");
        return 1;
    }
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0 || TTF_Init() != 0) {
        fprintf(stderr, "[ffta] SDL init failed: %s\n", SDL_GetError());
        return 1;
    }
    const bool windowed = getenv("FFTA_WINDOWED") != NULL || SDL_GetNumVideoDisplays() < 2;
    Uint32 flags = SDL_WINDOW_SHOWN;
    int px = SDL_WINDOWPOS_UNDEFINED, py = SDL_WINDOWPOS_UNDEFINED;
    if (!windowed) {
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_BORDERLESS;
        px = SDL_WINDOWPOS_UNDEFINED_DISPLAY(1);
        py = SDL_WINDOWPOS_UNDEFINED_DISPLAY(1);
    }
    /* the sway config moves windows titled "...Bottom Screen" to DSI-1 */
    sWin = SDL_CreateWindow("Final Fantasy Tactics Advance - Bottom Screen", px, py, W, H, flags);
    if (!sWin) {
        fprintf(stderr, "[ffta] window failed: %s\n", SDL_GetError());
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
        fprintf(stderr, "[ffta] cannot load font %s: %s\n", fontPath, TTF_GetError());
        return 1;
    }
    sTouchFd = OpenBottomTouch();

    /* QA: FFTA_TEST_TAPS="ms,x,y;ms,x,y" injects taps through the real tap handler */
    struct { unsigned ms; int x, y; bool done; } taps[32];
    int tapCount = 0;
    if (getenv("FFTA_TEST_TAPS")) {
        char* spec = strdup(getenv("FFTA_TEST_TAPS"));
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
        /* dragging a list moves it one row per row-height of travel */
        static int dragAcc;
        dragAcc += sDragDy;
        sDragDy = 0;
        const int rowH = sTab == TAB_PARTY ? UROW_H : IROW_H;
        while (dragAcc <= -rowH) { dragAcc += rowH; if (sTab == TAB_PARTY) sUnitScroll++; else if (sTab == TAB_ITEMS) sItemScroll++; }
        while (dragAcc >= rowH) { dragAcc -= rowH; if (sTab == TAB_PARTY) sUnitScroll--; else if (sTab == TAB_ITEMS) sItemScroll--; }
        if (sPendingTap) {
            sPendingTap = false;
            OnTap(sTapX, sTapY);
        }
        for (int i = 0; i < tapCount; i++)
            if (!taps[i].done && SDL_GetTicks() - startMs >= taps[i].ms) {
                taps[i].done = true;
                fprintf(stderr, "[ffta] test tap (%d,%d)\n", taps[i].x, taps[i].y);
                OnTap(taps[i].x, taps[i].y);
            }
        const Uint32 now = SDL_GetTicks();
        if (now - lastPoll >= 200) {
            lastPoll = now;
            if (PollState()) {
                if (!sConnected)
                    fprintf(stderr, "[ffta] connected to RetroArch\n");
                sConnected = true;
                silent = 0;
            } else if (++silent > 5u * (sConnected ? MAX_SILENT_SECONDS : STARTUP_GRACE_SECONDS)) {
                fprintf(stderr, "[ffta] RetroArch is gone, exiting\n");
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
