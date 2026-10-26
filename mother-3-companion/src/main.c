/* m3-bottom: touch companion for Mother 3 (fan translation) running in RetroArch's mGBA core on the RG DS.
 * Runs on the bottom panel (DSI-1). Reads the party over RetroArch's network command interface; tapping a healing item in
 * the GOODS tab drives the game's own Goods menu through RetroArch's network gamepad. */
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

#include "m3_data.h"
#include "m3_items.h"
#include "ra_client.h"

#define W 640
#define H 480
#define HDR_H 58
#define TAB_Y 424
#define TAB_H 56
#define MAX_SILENT_SECONDS 20
#define STARTUP_GRACE_SECONDS 180

#include "ui_common.h"

/* the game's own look: pink-beige windows with dark ink and orange highlights */
static const Col kInk = { 0, 0, 0 }, kPaper = { 231, 211, 206 }, kPaper2 = { 200, 176, 172 }, kOrange = { 255, 146, 16 },
                 kWhite = { 255, 251, 255 }, kBrown = { 74, 40, 33 }, kYellow = { 255, 251, 165 }, kPink = { 255, 16, 115 },
                 kHp = { 255, 146, 16 }, kPp = { 189, 243, 255 }, kDark = { 231, 211, 206 }, kCard = { 244, 232, 228 }, kMute = { 123, 73, 49 };

typedef struct {
    bool present;
    int id;                 /* index of the save record (0 Lucas, 1 Duster, 2 Kumatora, 3 Boney, ...) or -1 */
    char name[NAME_LEN + 1];
    int level, hp, maxHp, pp, maxPp, off, def, iq, spd;
    unsigned exp;
    uint8_t equip[4], items[16];
} Member;
static Member sM[PARTY_SLOTS];
static unsigned sDp;
static bool sConnected, sValid;

enum { TAB_PARTY, TAB_GOODS, TAB_EQUIP, TAB_COUNT };
static const char* const kTabName[TAB_COUNT] = { "PARTY", "GOODS", "EQUIP" };
static const char* const kTabTitle[TAB_COUNT] = { "Status", "Goods", "Equip" };
enum { H_MODE = 90, H_PSI = 400, H_TARGET = 420, H_TARGET_BACK = 419, H_TAB = 100, H_WHO = 120, H_SLOT = 140, H_PICK = 300, H_PICK_BACK = 299, H_PICK_MORE = 298, H_ITEM = 200 };
static int sPickPage; /* picker page, 6 items each */
static int sPick = -1; /* equipment slot being changed, or -1 */
static int sTab = TAB_PARTY, sWho;
static int sGoodsMode;  /* 0 = items, 1 = PSI usable from the pause menu */
static int sPsiSel = -1; /* PSI row waiting for a target, or -1 */
static bool sBusy;
static const char* sBusyMsg = "";

static uint16_t U16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t U32(const uint8_t* p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

static void DecodeName(const uint8_t* p, char* out) {
    int n = 0;
    for (int i = 0; i < NAME_LEN; i++) {
        const int c = U16(p + i * 2);
        if (c == 0xFFFF) break;
        char ch = '?';
        if (c >= 0x21 && c <= 0x3A) ch = (char)('A' + c - 0x21);
        else if (c >= 0x41 && c <= 0x5A) ch = (char)('a' + c - 0x41);
        else if (c >= 0x10 && c <= 0x19) ch = (char)('0' + c - 0x10);
        else if (c == 0x40) ch = ' ';
        out[n++] = ch;
    }
    out[n] = 0;
}

static bool PollState(void) {
    static uint8_t rec[0x6C * CHAR_COUNT + 0x30];
    uint8_t hp[0x70], st[SLOT_STRIDE * PARTY_SLOTS], dp[4];
    const uint32_t recBase = CHAR_BASE - 0x20;
    if (!RaClient_Read(recBase, rec, sizeof rec) || !RaClient_Read(0x0200DE00u, hp, sizeof hp) ||
        !RaClient_Read(SLOT_STATS, st, sizeof st) || !RaClient_Read(ADDR_DP, dp, 4))
        return false;
    sDp = U32(dp);
    for (int s = 0; s < PARTY_SLOTS; s++) {
        Member* m = &sM[s];
        memset(m, 0, sizeof *m);
        const uint8_t* q = st + s * SLOT_STRIDE;
        m->maxHp = U16(q + 8);
        if (m->maxHp == 0) continue;
        m->present = true;
        m->hp = U16(hp + (ADDR_HP_LIVE - 0x0200DE00u) + 0x0C * s);
        m->pp = U16(hp + (ADDR_PP_LIVE - 0x0200DE00u) + 0x0C * s);
        m->maxPp = U16(q + 0x10);
        m->off = U16(q + 0x16);
        m->def = U16(q + 0x1C);
        m->iq = U16(q + 0x22);
        m->spd = U16(q + 0x28);
        /* the save record whose max HP matches this party slot */
        m->id = -1;
        for (int c = 0; c < CHAR_COUNT; c++) {
            const uint8_t* r = rec + 0x20 + c * CHAR_STRIDE;
            if ((int)U32(r + CH_MAXHP) == m->maxHp && U32(r) <= (uint32_t)m->maxHp + 1000) { m->id = c; break; }
        }
        /* max HP can drift from the stats struct (buffs, equipment): fall back to the record holding this slot's live HP and PP */
        for (int c = 0; c < CHAR_COUNT && m->id < 0; c++) {
            const uint8_t* r = rec + 0x20 + c * CHAR_STRIDE;
            if (U32(r) == m->hp && U32(r + 4) == m->pp && U32(r + CH_MAXHP) > 0) m->id = c;
        }
        if (m->id >= 0) {
            const uint8_t* r = rec + 0x20 + m->id * CHAR_STRIDE;
            DecodeName(r + CH_NAME, m->name);
            m->level = r[CH_LEVEL];
            m->exp = U32(r + CH_EXP);
            memcpy(m->equip, r + CH_EQUIP, 4);
            memcpy(m->items, r + CH_ITEMS, 16);
        } else {
            snprintf(m->name, sizeof m->name, "Party %d", s + 1);
        }
    }
    sValid = sM[0].present && sM[0].maxHp < 5000 && sM[0].hp <= sM[0].maxHp;
    return true;
}

static void Bar(int x, int y, int w, int cur, int max, Col c) {
    FillRect(x - 2, y - 2, w + 4, 14, kBrown);
    FillRect(x, y, w, 10, kPaper2);
    if (max > 0) FillRect(x, y, w * (cur > max ? max : cur) / max, 10, c);
}

/* the pause menu's window frame: brown line, white line, then the beige fill */
static void Window(int x, int y, int w, int h, Col fill) {
    FillRect(x, y, w, h, kBrown);
    FillRect(x + 3, y + 3, w - 6, h - 6, kWhite);
    FillRect(x + 7, y + 7, w - 14, h - 14, fill);
}

static const M3Item* Item(int id) { return &kItems[id < (int)(sizeof kItems / sizeof kItems[0]) ? id : 0]; }
static bool Empty(int id) { return id == 0 || strcmp(Item(id)->name, "EMPTY") == 0 || Item(id)->name[0] == 0; }
static bool Healer(int id);
static bool Healer(int id) { return !Empty(id) && (Item(id)->hp || Item(id)->pp); }

static void DrawCard(int s, int x, int y) {
    const Member* m = &sM[s];
    const int w = 308, h = 174;
    Window(x, y, w, h, kCard);
    if (!m->present) return;
    Text(sFont22, x + 18, y + 14, 0, kInk, "%s", m->name);
    Text(sFont14, x + w - 18, y + 20, 2, kInk, "Lv %d", m->level);
    Text(sFont14, x + 18, y + 50, 0, kInk, "HP");
    Text(sFont18, x + w - 18, y + 46, 2, kInk, "%d / %d", m->hp, m->maxHp);
    Bar(x + 20, y + 72, w - 40, m->hp, m->maxHp, kHp);
    if (m->maxPp > 0) {
        Text(sFont14, x + 18, y + 92, 0, kInk, "PP");
        Text(sFont18, x + w - 18, y + 88, 2, kInk, "%d / %d", m->pp, m->maxPp);
        Bar(x + 20, y + 114, w - 40, m->pp, m->maxPp, kPp);
    }
    Text(sFont12, x + 18, y + 136, 0, kInk, "OFF %d   DEF %d   IQ %d   SPD %d", m->off, m->def, m->iq, m->spd);
    Text(sFont12, x + 18, y + 154, 0, kMute, "EXP %u", m->exp);
}

static void DrawParty(void) {
    for (int s = 0; s < PARTY_SLOTS; s++) DrawCard(s, 8 + (s % 2) * 316, HDR_H + 6 + (s / 2) * 180);
}

static bool Equipped(const Member* m, int slot) {
    const int id = m->items[slot];
    for (int k = 0; k < 4; k++)
        if (m->equip[k] == id && id != 0) return true;
    return false;
}

static void DrawSelector(void) {
    const int bw = (W - 16) / PARTY_SLOTS;
    Window(8, HDR_H + 4, W - 16, 38, kCard);
    for (int s = 0; s < PARTY_SLOTS; s++) {
        const bool act = s == sWho, on = sM[s].present;
        const int x = 8 + s * bw;
        if (act) {
            FillRect(x + 6, HDR_H + 9, bw - 12, 28, kBrown);
            FillRect(x + 8, HDR_H + 11, bw - 16, 24, kOrange);
        } else if (s > 0) {
            for (int y = HDR_H + 12; y < HDR_H + 34; y += 8) FillRect(x - 1, y, 3, 4, kPaper2);
        }
        Text(sFont18, x + bw / 2, HDR_H + 12, 1, on ? kInk : kMute, "%s", on ? sM[s].name : "-");
        if (on) AddHit(x, HDR_H + 4, bw, 38, H_WHO + s);
    }
}

/* the PSI the pause menu can cast on the field (the rest are greyed out in the game): Lifeup and Healing */
typedef struct { int id, cost; bool all; const char *name, *desc; } PsiInfo;
static const PsiInfo kPsi[] = {
    { 40, 5, false, "Lifeup \xCE\xB1", "Restores about 60 HP to one ally." },
    { 41, 9, false, "Lifeup \xCE\xB2", "Restores about 120 HP to one ally." },
    { 42, 16, false, "Lifeup \xCE\xB3", "Fully restores one ally's HP." },
    { 43, 28, true, "Lifeup \xCE\xA9", "Restores about 240 HP to all allies." },
    { 45, 4, false, "Healing \xCE\xB1", "Cures one status ailment an ally has." },
    { 46, 8, false, "Healing \xCE\xB2", "Cures all status ailments an ally has." },
    { 47, 18, false, "Healing \xCE\xB3", "Cures all ailments, even incapacitation." },
    { 48, 30, true, "Healing \xCE\xA9", "Cures all allies' ailments, even incapacitation." },
};
#define PSI_KINDS ((int)(sizeof kPsi / sizeof kPsi[0]))
typedef struct { int kind, pos; } PsiKnown; /* kPsi index and the spell's position in the game's PSI list */
static PsiKnown sPsiKnown[PARTY_SLOTS][PSI_KINDS];
static int sPsiCount[PARTY_SLOTS];
static bool sPsiScanned;
static int sPsiScanLevels[PARTY_SLOTS];

static int PsiKind(int id) {
    for (int k = 0; k < PSI_KINDS; k++)
        if (kPsi[k].id == id) return k;
    return -1;
}

static void DrawPsi(void) {
    DrawSelector();
    const Member* m = &sM[sWho];
    const int y0 = HDR_H + 52, rh = 44, lh = 36;
    Window(8, y0 - 6, W - 16, 8 * 36 + 22, kCard);
    if (!m->present) return;
    if (sPsiSel >= 0) {
        const PsiInfo* pi = &kPsi[sPsiKnown[sWho][sPsiSel].kind];
        Text(sFont18, 44, y0 + 6, 0, kInk, "%s: cast on whom?", pi->name);
        FillRect(W - 144, y0 + 2, 100, 30, kPaper2);
        FrameRect(W - 144, y0 + 2, 100, 30, kBrown);
        Text(sFont14, W - 94, y0 + 9, 1, kInk, "Back");
        AddHit(W - 144, y0 + 2, 100, 30, H_TARGET_BACK);
        for (int s = 0; s < PARTY_SLOTS; s++) {
            if (!sM[s].present) continue;
            const int y = y0 + 42 + s * rh;
            FillRect(40, y, W - 80, rh - 4, kYellow);
            FrameRect(40, y, W - 80, rh - 4, kBrown);
            Text(sFont18, 56, y + 9, 0, kInk, "%s", sM[s].name);
            Text(sFont14, W - 56, y + 12, 2, kInk, "HP %d / %d", sM[s].hp, sM[s].maxHp);
            AddHit(40, y, W - 80, rh - 4, H_TARGET + s);
        }
        return;
    }
    if (sPsiCount[sWho] == 0) {
        Text(sFont18, W / 2, y0 + 70, 1, kMute, m->maxPp > 0 ? (sPsiScanned ? "No usable PSI" : "Reading PSI...") : "No PSI");
        return;
    }
    for (int i = 0; i < sPsiCount[sWho]; i++) {
        const PsiInfo* pi = &kPsi[sPsiKnown[sWho][i].kind];
        const int y = y0 + 4 + i * lh;
        const bool ok = m->pp >= pi->cost;
        FillRect(20, y, W - 40, lh - 4, ok ? kYellow : kPaper2);
        FrameRect(20, y, W - 40, lh - 4, kBrown);
        Text(sFont18, 36, y + 5, 0, ok ? kInk : kMute, "%s", pi->name);
        Text(sFont12, 190, y + 9, 0, kMute, "%s", pi->desc);
        Text(sFont14, W - 36, y + 8, 2, ok ? kInk : kMute, "%d PP", pi->cost);
        if (ok) AddHit(20, y, W - 40, lh - 4, H_PSI + i);
    }
}

static void DrawGoods(void) {
    if (sGoodsMode == 1) { DrawPsi(); return; }
    DrawSelector();
    const Member* m = &sM[sWho];
    const int y0 = HDR_H + 52, rh = 36, cw = (W - 16 - 8) / 2;
    Window(8, y0 - 6, W - 16, 8 * rh + 22, kCard);
    for (int y = y0 + 4; y < y0 + 8 * rh + 4; y += 10) FillRect(W / 2 - 2, y, 4, 5, kWhite); /* the dashed divider */
    if (!m->present) return;
    for (int i = 0; i < 16; i++) {
        const int id = m->items[i];
        if (Empty(id)) continue;
        const int x = 20 + (i % 2) * (cw + 4), y = y0 + 4 + (i / 2) * rh;
        const bool heal = Healer(id);
        if (heal) {
            FillRect(x - 4, y, cw - 28, rh - 4, kYellow);
            FrameRect(x - 4, y, cw - 28, rh - 4, kBrown);
            AddHit(x - 4, y, cw - 28, rh - 4, H_ITEM + i);
        }
        int tx = x + 26;
        if (Equipped(m, i)) { /* the game's circled E */
            FillRect(x, y + 6, 20, 20, kBrown);
            FillRect(x + 2, y + 8, 16, 16, kWhite);
            FillRect(x + 4, y + 10, 12, 12, kBrown);
            Text(sFont14, x + 10, y + 8, 1, kWhite, "E");
        }
        Text(sFont18, tx, y + 5, 0, kInk, "%s", Item(id)->name);
        if (heal) Text(sFont14, x + cw - 40, y + 8, 2, kInk, "+%d %s", Item(id)->hp ? Item(id)->hp : Item(id)->pp, Item(id)->hp ? "HP" : "PP");
    }
}

static const char* const kSlotName[4] = { "Weapon", "Body", "Head", "Other" };

/* weapons only fit the character they were made for: sticks and bats (Lucas), gloves (Kumatora), shoes (Duster), dog weapons (Boney) */
static int WeaponFamily(int id) {
    if ((id >= 1 && id <= 12) || (id >= 34 && id <= 36)) return 0;
    if (id >= 13 && id <= 21) return 1;
    if (id >= 22 && id <= 30) return 2;
    if (id == 31 || id == 174) return 3;
    if (id == 32) return 4;
    if (id == 33) return 5;
    return 6 + id;
}
static bool Fits(const Member* m, int slot, int id) {
    if (Empty(id) || Item(id)->slot != slot + 1) return false;
    const bool dog = m->id == 3;
    if (slot == 0) {
        const int cur = m->equip[0], base = m->id == 0 ? 0 : m->id == 1 ? 2 : m->id == 2 ? 1 : 4;
        return WeaponFamily(id) == (cur ? WeaponFamily(cur) : base);
    }
    if (slot == 1) return dog ? (id >= 46 && id <= 50) : !(id >= 46 && id <= 50);
    return !dog;
}

/* writes the new item id into the character's equipment, then opens and leaves the game's Equip screen, which recalculates the stats */
static void SetEquip(int who, int slot, int id);

static int PickList(const Member* m, int slot, int* ids) {
    int n = 0;
    for (int i = 0; i < 16; i++) {
        const int id = m->items[i];
        if (!Fits(m, slot, id)) continue;
        bool dup = false;
        for (int k = 0; k < n; k++) dup |= ids[k] == id;
        if (!dup && n < 9) ids[n++] = id;
    }
    ids[n++] = 0; /* None */
    return n;
}

static void DrawPicker(const Member* m) {
    int ids[10];
    const int n = PickList(m, sPick, ids);
    const int y0 = HDR_H + 52, rh = 44, per = 6;
    if (sPickPage * per >= n) sPickPage = 0;
    Window(8, y0 - 6, W - 16, 4 * 64 + 54, kCard);
    Text(sFont18, 44, y0 + 6, 0, kInk, "%s: choose an item", kSlotName[sPick]);
    FillRect(W - 144, y0 + 2, 100, 30, kPaper2);
    FrameRect(W - 144, y0 + 2, 100, 30, kBrown);
    Text(sFont14, W - 94, y0 + 9, 1, kInk, "Back");
    AddHit(W - 144, y0 + 2, 100, 30, H_PICK_BACK);
    if (n > per) {
        FillRect(W - 264, y0 + 2, 110, 30, kPaper2);
        FrameRect(W - 264, y0 + 2, 110, 30, kBrown);
        Text(sFont14, W - 209, y0 + 9, 1, kInk, "More (%d/%d)", sPickPage + 1, (n + per - 1) / per);
        AddHit(W - 264, y0 + 2, 110, 30, H_PICK_MORE);
    }
    for (int i = sPickPage * per; i < n && i < (sPickPage + 1) * per; i++) {
        const int id = ids[i], y = y0 + 42 + (i - sPickPage * per) * rh;
        const bool cur = id == m->equip[sPick] || (id == 0 && m->equip[sPick] == 0);
        FillRect(40, y, W - 80, rh - 4, cur ? kYellow : kPaper2);
        FrameRect(40, y, W - 80, rh - 4, kBrown);
        Text(sFont18, 56, y + 9, 0, kInk, "%s", id ? Item(id)->name : "None");
        if (id) {
            char d[64];
            snprintf(d, sizeof d, "%.46s", Item(id)->desc);
            Text(sFont12, W - 56, y + 13, 2, kMute, "%s", d);
        }
        AddHit(40, y, W - 80, rh - 4, H_PICK + i);
    }
}

static void DrawEquip(void) {
    DrawSelector();
    const Member* m = &sM[sWho];
    if (sPick >= 0 && m->present) { DrawPicker(m); return; }
    const int y0 = HDR_H + 52, rh = 64;
    Window(8, y0 - 6, W - 16, 4 * rh + 54, kCard);
    if (!m->present) return;
    for (int k = 0; k < 4; k++) {
        const int y = y0 + 4 + k * rh, id = m->equip[k];
        AddHit(16, y - 2, W - 32, rh - 8, H_SLOT + k);
        Text(sFont14, 44, y + 8, 0, kMute, "%s", kSlotName[k]);
        Text(sFont12, W - 44, y + 10, 2, kOrange, "tap to change");
        if (Empty(id)) {
            Text(sFont18, 160, y + 4, 0, kMute, "(nothing)");
        } else {
            Text(sFont18, 160, y + 4, 0, kInk, "%s", Item(id)->name);
            char line[2][100] = { "", "" };
            const char* d = Item(id)->desc;
            const int len = (int)strlen(d);
            int cut = len > 56 ? 56 : len;
            if (cut < len) while (cut > 30 && d[cut] != ' ') cut--;
            snprintf(line[0], sizeof line[0], "%.*s", cut, d);
            if (cut < len) snprintf(line[1], sizeof line[1], "%s", d + cut + 1);
            Text(sFont12, 160, y + 28, 0, kMute, "%s", line[0]);
            if (line[1][0]) Text(sFont12, 160, y + 41, 0, kMute, "%s", line[1]);
        }
        if (k < 3) FillRect(40, y + rh - 4, W - 80, 2, kPaper2);
    }
    const int sy = y0 + 4 * rh + 4;
    FillRect(40, sy - 6, W - 80, 2, kBrown);
    Text(sFont18, 44, sy + 4, 0, kInk, "Offense %d    Defense %d    IQ %d    Speed %d", m->off, m->def, m->iq, m->spd);
}

/* one window holds every tab; the active one is filled orange */
static void DrawTabs(void) {
    const int tw = (W - 16) / TAB_COUNT;
    Window(4, TAB_Y, W - 8, TAB_H, kCard);
    for (int t = 0; t < TAB_COUNT; t++) {
        const int x = 8 + t * tw;
        if (t == sTab) {
            FillRect(x + 4, TAB_Y + 8, tw - 8, TAB_H - 16, kBrown);
            FillRect(x + 6, TAB_Y + 10, tw - 12, TAB_H - 20, kOrange);
        } else if (t > 0) {
            for (int y = TAB_Y + 12; y < TAB_Y + TAB_H - 12; y += 8) FillRect(x - 2, y, 3, 4, kPaper2);
        }
        Text(sFont18, x + tw / 2, TAB_Y + 17, 1, kInk, "%s", kTabName[t]);
        AddHit(x, TAB_Y, tw, TAB_H, H_TAB + t);
    }
}

static void DrawFrame(void) {
    FillRect(0, 0, W, H, kDark);
    sHitCount = 0;
    if (!sConnected) {
        Text(sFont22, W / 2, 170, 1, kInk, "MOTHER 3");
        Text(sFont18, W / 2, 214, 1, kInk, "Waiting for RetroArch...");
    } else if (!sValid) {
        Text(sFont22, W / 2, 170, 1, kInk, "MOTHER 3");
        Text(sFont18, W / 2, 214, 1, kInk, "Load a saved game");
    } else {
        Text(sFont22, 24, 17, 0, kInk, "%s", kTabTitle[sTab]);
        if (sTab == TAB_GOODS) {
            for (int k = 0; k < 2; k++) {
                const int x = 130 + k * 90;
                FillRect(x, 10, 82, 34, kBrown);
                FillRect(x + 2, 12, 78, 30, k == sGoodsMode ? kOrange : kCard);
                Text(sFont14, x + 41, 19, 1, kInk, k ? "PSI" : "Items");
                AddHit(x, 6, 82, 42, H_MODE + k);
            }
        }
        Text(sFont18, W - 24, 18, 2, kInk, "%u DP", sDp);
        if (sTab == TAB_PARTY) DrawParty(); else if (sTab == TAB_GOODS) DrawGoods(); else DrawEquip();
        DrawTabs();
        if (sBusy) {
            FillRect(0, 0, W, H, kDark);
            Window(110, 180, 420, 120, kCard);
            Text(sFont22, W / 2, 214, 1, kInk, "%s", sBusyMsg);
            Text(sFont12, W / 2, 256, 1, kMute, "driving the game's menu on the top screen");
        }
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
enum { PAD_B = 0, PAD_START = 3, PAD_UP = 4, PAD_DOWN = 5, PAD_LEFT = 6, PAD_RIGHT = 7, PAD_A = 8, PAD_R = 11 };

/* 0x02005C00: 0 on the field, 14 main menu, 10 Goods and its Use / target prompts */
static int MenuState(void) {
    uint8_t f = 0xFF;
    return RaClient_Read(ADDR_MENU, &f, 1) ? f : -1;
}
static bool WaitMenu(int want) {
    for (int n = 0; n < 20; n++) {
        if (MenuState() == want) {
            SDL_Delay(500); /* the window slides in and ignores input until it settles */
            return true;
        }
        SDL_Delay(120);
    }
    return false;
}
static void CloseMenu(void) {
    for (int n = 0; n < 8 && MenuState() != 0; n++) Pad(PAD_B, 450);
}

/* Start > Goods > the character's tab > the item > Use > the character, then back out to the field. */
static void UseItem(int who, int slot) {
    if (!sM[who].present || sM[who].id < 0 || !Healer(sM[who].items[slot])) return;
    const int before = sM[who].items[slot];
    sBusy = true;
    sBusyMsg = "Using item...";
    DrawFrame();
    CloseMenu();
    Pad(PAD_START, 100);
    if (WaitMenu(14)) {
        Pad(PAD_A, 100);
        if (WaitMenu(10)) {
            for (int n = 0; n < who; n++) Pad(PAD_R, 350);
            if (slot % 2) Pad(PAD_RIGHT, 250);
            for (int n = 0; n < slot / 2; n++) Pad(PAD_DOWN, 250);
            Pad(PAD_A, 500); /* opens Use / Give / Drop */
            Pad(PAD_A, 500); /* Use -> On whom? */
            for (int n = 0; n < who; n++) Pad(PAD_DOWN, 250);
            Pad(PAD_A, 1200);
        }
    }
    CloseMenu();
    (void)before;
    sBusy = false;
}


/* Start > PSI > the character's tab > the spell > (On whom?) > back out. `target` < 0 casts a spell that hits everyone. */
static void OpenPsiScreen(void) {
    Pad(PAD_START, 100);
    if (!WaitMenu(14)) return;
    for (int n = 0; n < 6; n++) {
        uint8_t cur = 0xFF;
        if (!RaClient_Read(0x02018CDCu, &cur, 1) || cur == 2) break;
        Pad(PAD_RIGHT, 300);
    }
    Pad(PAD_A, 100);
    WaitMenu(10);
}

static void UsePsi(int who, int idx, int target) {
    const PsiKnown* k = &sPsiKnown[who][idx];
    sBusy = true;
    sBusyMsg = "Casting PSI...";
    DrawFrame();
    CloseMenu();
    OpenPsiScreen();
    if (MenuState() == 10) {
        for (int n = 0; n < who; n++) Pad(PAD_R, 350);
        if (k->pos % 2) Pad(PAD_RIGHT, 250);
        for (int n = 0; n < k->pos / 2; n++) Pad(PAD_DOWN, 250);
        Pad(PAD_A, 600);
        if (target >= 0) {
            for (int n = 0; n < target; n++) Pad(PAD_DOWN, 250);
            Pad(PAD_A, 1200);
        } else {
            SDL_Delay(900);
        }
    }
    CloseMenu();
    sBusy = false;
}

/* The PSI screen builds its rows as {ROM name pointer, 0, column, row, ...} records at 0x02021B40 (0x14 bytes each), in list
 * order. Only characters with PP have PSI (the records of the others are stale), and Lifeup and Healing come first. */
static void ScanPsi(void) {
    sBusy = true;
    sBusyMsg = "Reading PSI...";
    DrawFrame();
    for (int s = 0; s < PARTY_SLOTS; s++) sPsiCount[s] = 0;
    CloseMenu();
    OpenPsiScreen();
    if (MenuState() == 10) {
        for (int who = 0; who < PARTY_SLOTS; who++) {
            if (who) Pad(PAD_R, 800);
            SDL_Delay(500);
            if (!sM[who].present || sM[who].maxPp <= 0) continue;
            uint8_t rows[0x14 * 12];
            if (!RaClient_Read(0x02021B40u, rows, sizeof rows)) continue;
            for (int e = 0; e < 12; e++) {
                const uint32_t ptr = U32(rows + e * 0x14);
                if (ptr < 0x08D28D84u || (ptr - 0x08D28D84u) % 0x28) break;
                const int kind = PsiKind((int)((ptr - 0x08D28D84u) / 0x28));
                if (kind >= 0 && sPsiCount[who] < PSI_KINDS) sPsiKnown[who][sPsiCount[who]++] = (PsiKnown){ kind, e };
            }
        }
        sPsiScanned = true;
        for (int s = 0; s < PARTY_SLOTS; s++) sPsiScanLevels[s] = sM[s].level;
    }
    CloseMenu();
    sBusy = false;
}

static void SetEquip(int who, int slot, int id) {
    const Member* m = &sM[who];
    if (!m->present || m->id < 0 || m->equip[slot] == id) return;
    sBusy = true;
    sBusyMsg = "Changing equipment...";
    DrawFrame();
    CloseMenu();
    const uint8_t b = (uint8_t)id;
    RaClient_Write(CHAR_BASE + (uint32_t)m->id * CHAR_STRIDE + CH_EQUIP + (uint32_t)slot, &b, 1);
    Pad(PAD_START, 100);
    if (WaitMenu(14)) {
        for (int n = 0; n < 6; n++) {
            uint8_t cur = 0xFF;
            if (!RaClient_Read(0x02018CDCu, &cur, 1) || cur == 1) break;
            Pad(PAD_RIGHT, 300);
        }
        Pad(PAD_A, 100);
        if (WaitMenu(10)) {
            for (int n = 0; n < who; n++) Pad(PAD_R, 400); /* the stats of the character being viewed are recalculated on leaving */
            Pad(PAD_B, 600);
            Pad(PAD_B, 600);
        }
    }
    CloseMenu();
    sBusy = false;
}

static void OnTap(int x, int y) {
    const int id = HitTest(x, y);
    if (id >= H_TAB && id < H_TAB + TAB_COUNT) { sTab = id - H_TAB; sPick = -1; sPsiSel = -1; }
    else if (id >= H_WHO && id < H_WHO + PARTY_SLOTS) { sWho = id - H_WHO; sPick = -1; sPsiSel = -1; }
    else if (id >= H_MODE && id < H_MODE + 2 && sTab == TAB_GOODS) {
        sGoodsMode = id - H_MODE;
        sPsiSel = -1;
        if (sGoodsMode == 1) {
            bool stale = !sPsiScanned;
            for (int s = 0; s < PARTY_SLOTS; s++) stale |= sM[s].level != sPsiScanLevels[s];
            if (stale) ScanPsi();
        }
    }
    else if (id >= H_PSI && id < H_PSI + PSI_KINDS && sTab == TAB_GOODS && sGoodsMode == 1) {
        if (id - H_PSI < sPsiCount[sWho]) {
            if (kPsi[sPsiKnown[sWho][id - H_PSI].kind].all) UsePsi(sWho, id - H_PSI, -1);
            else sPsiSel = id - H_PSI;
        }
    }
    else if (id == H_TARGET_BACK) sPsiSel = -1;
    else if (id >= H_TARGET && id < H_TARGET + PARTY_SLOTS && sPsiSel >= 0) { const int i = sPsiSel; sPsiSel = -1; UsePsi(sWho, i, id - H_TARGET); }
    else if (id >= H_ITEM && id < H_ITEM + 16 && sTab == TAB_GOODS) UseItem(sWho, id - H_ITEM);
    else if (id >= H_SLOT && id < H_SLOT + 4 && sTab == TAB_EQUIP) { sPick = id - H_SLOT; sPickPage = 0; }
    else if (id == H_PICK_BACK) sPick = -1;
    else if (id == H_PICK_MORE) sPickPage++;
    else if (id >= H_PICK && id < H_PICK + 10 && sTab == TAB_EQUIP && sPick >= 0) {
        int ids[10];
        const int n = PickList(&sM[sWho], sPick, ids);
        if (id - H_PICK < n) SetEquip(sWho, sPick, ids[id - H_PICK]);
        sPick = -1;
    }
}

int main(int argc, char** argv) {
    (void)argc;
    char dir[512] = ".";
    if (strrchr(argv[0], '/')) {
        snprintf(dir, sizeof dir, "%s", argv[0]);
        *strrchr(dir, '/') = 0;
    }
    if (getenv("M3_DATA_DIR"))
        snprintf(dir, sizeof dir, "%s", getenv("M3_DATA_DIR"));
    if (!RaClient_Open(getenv("M3_HOST") ? getenv("M3_HOST") : "127.0.0.1", 55355)) {
        fprintf(stderr, "[m3] cannot open the RetroArch command socket\n");
        return 1;
    }
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0 || TTF_Init() != 0) {
        fprintf(stderr, "[m3] SDL init failed: %s\n", SDL_GetError());
        return 1;
    }
    const bool windowed = getenv("M3_WINDOWED") != NULL || SDL_GetNumVideoDisplays() < 2;
    Uint32 flags = SDL_WINDOW_SHOWN;
    int px = SDL_WINDOWPOS_UNDEFINED, py = SDL_WINDOWPOS_UNDEFINED;
    if (!windowed) {
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_BORDERLESS;
        px = SDL_WINDOWPOS_UNDEFINED_DISPLAY(1);
        py = SDL_WINDOWPOS_UNDEFINED_DISPLAY(1);
    }
    /* the sway config moves windows titled "...Bottom Screen" to DSI-1 */
    sWin = SDL_CreateWindow("Mother 3 - Bottom Screen", px, py, W, H, flags);
    if (!sWin) {
        fprintf(stderr, "[m3] window failed: %s\n", SDL_GetError());
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
        fprintf(stderr, "[m3] cannot load font %s: %s\n", fontPath, TTF_GetError());
        return 1;
    }
    sTouchFd = OpenBottomTouch();

    /* QA: M3_TEST_TAPS="ms,x,y;ms,x,y" injects taps through the real tap handler */
    struct { unsigned ms; int x, y; bool done; } taps[32];
    int tapCount = 0;
    if (getenv("M3_TEST_TAPS")) {
        char* spec = strdup(getenv("M3_TEST_TAPS"));
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
                fprintf(stderr, "[m3] test tap (%d,%d)\n", taps[i].x, taps[i].y);
                OnTap(taps[i].x, taps[i].y);
            }
        const Uint32 now = SDL_GetTicks();
        if (now - lastPoll >= 200) {
            lastPoll = now;
            uint8_t code[4] = { 0 };
            if (RaClient_Read(M3_ROM_CODE_ADDR, code, 4) && memcmp(code, "A3UJ", 4) == 0 && PollState()) {
                if (!sConnected)
                    fprintf(stderr, "[m3] connected to RetroArch\n");
                sConnected = true;
                silent = 0;
            } else if (++silent > 5u * (sConnected ? MAX_SILENT_SECONDS : STARTUP_GRACE_SECONDS)) {
                fprintf(stderr, "[m3] RetroArch is gone, exiting\n");
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
