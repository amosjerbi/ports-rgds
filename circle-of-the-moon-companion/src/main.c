/* cotm-bottom: touch companion for Castlevania: Circle of the Moon running in RetroArch's mGBA core on the RG DS.
 * Runs on the bottom panel (DSI-1). Reads the game's RAM and ROM over RetroArch's network command interface
 * (READ_CORE_MEMORY, UDP 55355) and writes equipment / DSS cards back (WRITE_CORE_MEMORY).
 * Tabs: DSS, EQUIP, ITEM, MAGIC, MAP. It never touches the emulator's video or input. */
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "cotm_data.h"
#include "ra_client.h"

#define W 640
#define H 480
#define HDR_H 58
#define TAB_Y 424
#define TAB_H 56
#define BODY_Y (HDR_H + 4)
#define ROW_H 38
#define MAX_SILENT_SECONDS 20
#define STARTUP_GRACE_SECONDS 180

/* ------------------------------------------------------------------------------------------------------------------ */
/* ROM text                                                                                                           */
/* ------------------------------------------------------------------------------------------------------------------ */
/* Text is one byte per character from a frequency-ordered table (e = 0x28, a = 0x29, t = 0x2A ...); 0x00 / 0x03 end a
 * string, 0x02 / 0x09 are line breaks, anything unknown (control codes, button glyphs) is skipped. */
static const char* sCh[256];

static void InitCharTable(void) {
    static const struct { int code; const char* s; } t[] = {
        { 0x26, " " }, { 0x27, " " }, { 0x28, "e" }, { 0x29, "a" }, { 0x2A, "t" }, { 0x2B, "o" }, { 0x2C, "r" }, { 0x2D, "i" },
        { 0x2E, "n" }, { 0x2F, "s" }, { 0x30, "l" }, { 0x31, "h" }, { 0x32, "d" }, { 0x33, "c" }, { 0x34, "u" }, { 0x36, "m" },
        { 0x37, "w" }, { 0x38, "p" }, { 0x39, "g" }, { 0x3A, "f" }, { 0x3B, "y" }, { 0x3C, "b" }, { 0x3D, "k" }, { 0x3F, "v" },
        { 0x57, "x" }, { 0x61, "q" }, { 0x62, "j" }, { 0x65, "z" },
        { 0x3E, "A" }, { 0x40, "H" }, { 0x41, "S" }, { 0x42, "M" }, { 0x43, "I" }, { 0x44, "C" }, { 0x45, "D" }, { 0x46, "R" },
        { 0x47, "T" }, { 0x49, "W" }, { 0x4B, "P" }, { 0x4D, "B" }, { 0x4E, "E" }, { 0x4F, "G" }, { 0x52, "N" }, { 0x53, "Y" },
        { 0x56, "F" }, { 0x5F, "L" }, { 0x60, "U" }, { 0x66, "K" }, { 0x67, "O" }, { 0x6B, "J" }, { 0x6E, "V" }, { 0x6F, "Z" },
        { 0x35, "." }, { 0x4A, "!" }, { 0x4C, "," }, { 0x48, "?" }, { 0x51, "'" }, { 0x58, "-" }, { 0x54, "(" }, { 0x55, ")" },
        { 0x50, "+" }, { 0x68, "%" }, { 0x70, "/" }, { 0x6D, "x" },
        { 0x64, "0" }, { 0x6A, "1" }, { 0x63, "2" }, { 0x6C, "3" }, { 0x71, "4" }, { 0x69, "5" }, { 0x72, "6" },
    };
    memset(sCh, 0, sizeof sCh);
    for (size_t i = 0; i < sizeof t / sizeof t[0]; i++)
        sCh[t[i].code] = t[i].s;
}

static void DecodeText(char* dst, size_t cap, const uint8_t* src, size_t n) {
    size_t o = 0;
    for (size_t i = 0; i < n && src[i] != 0 && src[i] != 3 && o + 2 < cap; i++) {
        if (src[i] == 2 || src[i] == 9) {
            if (o && dst[o - 1] != ' ')
                dst[o++] = ' ';
        } else if (sCh[src[i]]) {
            const char c = sCh[src[i]][0];
            if (c == ' ' && o && dst[o - 1] == ' ')
                continue;
            dst[o++] = c;
        }
    }
    while (o && dst[o - 1] == ' ')
        o--;
    dst[o] = 0;
}

/* ------------------------------------------------------------------------------------------------------------------ */
/* game data                                                                                                          */
/* ------------------------------------------------------------------------------------------------------------------ */
static char sName[ITEM_LAST + 1][64];
static char sDesc[ITEM_LAST + 1][120];
static char sComboDesc[101][130];
static char sCardDesc[20][200];
static char sMagicDesc[8][200];
static int sEqStat[ITEM_LAST + 1][4];
static bool sRomLoaded;

static const char* kActName[10] = { "Mercury", "Venus", "Jupiter", "Mars", "Diana", "Apollo", "Neptune", "Saturn", "Uranus", "Pluto" };
static const char* kAttrName[10] = { "Salamander", "Serpent", "Mandragora", "Golem", "Cockatrice", "Manticore", "Griffin",
                                     "Thunderbird", "Unicorn", "Black Dog" };
static const char* kActAbbr[10] = { "Mer", "Ven", "Jup", "Mar", "Dia", "Apo", "Nep", "Sat", "Ura", "Plu" };
static const char* kAttrAbbr[10] = { "Sal", "Ser", "Man", "Gol", "Coc", "Mtc", "Gri", "Thu", "Uni", "Dog" };
static const char* kMagicName[8] = { "Dash Boots", "Double", "Tackle", "Kick Boots", "Heavy Ring", "Cleansing", "Roc Wing", "Last Key" };

/* the nth 0-terminated string at `p`, decoded into dst */
static const uint8_t* NextString(const uint8_t* p, const uint8_t* end, char* dst, size_t cap) {
    const uint8_t* e = p;
    while (e < end && *e)
        e++;
    DecodeText(dst, cap, p, (size_t)(e - p));
    return e < end ? e + 1 : end;
}

static bool LoadRom(void) {
    const size_t len = ROM_TEXT_HI - ROM_TEXT_LO;
    uint8_t* t = malloc(len);
    uint8_t st[8 * (ITEM_LAST + 1)];
    bool ok = t && RaClient_Read(ROM_TEXT_LO, t, len) && RaClient_Read(ROM_EQUIP_STATS, st, sizeof st);
    if (ok) {
        const uint8_t* end = t + len;
        const uint8_t* p = t + (ROM_NAMES - ROM_TEXT_LO);
        char tmp[64];
        for (int i = 0; i < ROM_NAME_COUNT; i++) {
            p = NextString(p, end, tmp, sizeof tmp);
            if (i == 0)
                snprintf(sName[0], sizeof sName[0], "%s", tmp);
            else if (i >= 3)
                snprintf(sName[i - 2], sizeof sName[0], "%s", tmp);
        }
        p = t + (ROM_DESCS - ROM_TEXT_LO);
        for (int i = 0; i <= ITEM_LAST; i++)
            p = NextString(p, end, sDesc[i], sizeof sDesc[0]);
        p = t + (ROM_COMBOS - ROM_TEXT_LO);
        for (int i = 0; i < 101; i++)
            p = NextString(p, end, sComboDesc[i], sizeof sComboDesc[0]);
        p = t + (ROM_CARDS - ROM_TEXT_LO);
        for (int i = 0; i < 20; i++)
            p = NextString(p, end, sCardDesc[i], sizeof sCardDesc[0]);
        p = t + (ROM_MAGIC - ROM_TEXT_LO);
        for (int i = 0; i < 8; i++)
            p = NextString(p, end, sMagicDesc[i], sizeof sMagicDesc[0]);
        for (int i = 0; i <= ITEM_LAST; i++)
            for (int k = 0; k < 4; k++)
                sEqStat[i][k] = (int16_t)(st[8 * i + 2 * k] | (st[8 * i + 2 * k + 1] << 8));
    }
    free(t);
    return ok;
}

/* ------------------------------------------------------------------------------------------------------------------ */
/* live state                                                                                                         */
/* ------------------------------------------------------------------------------------------------------------------ */
typedef struct {
    uint8_t sv[SAVE_SIZE];
    bool valid;
    int level, hp, maxHp, mp, maxMp, hearts, maxHearts, str, def, intl, lck;
    uint32_t exp, frames;
    int act, attr; /* equipped DSS cards, 0..9 or -1 */
    int body, arm[2];
} State;
static State sSt;

static uint16_t S16(const State* s, unsigned o) { return (uint16_t)(s->sv[o] | (s->sv[o + 1] << 8)); }
static uint32_t S32(const State* s, unsigned o) { return (uint32_t)S16(s, o) | ((uint32_t)S16(s, o + 2) << 16); }

static bool PollState(void) {
    State n;
    memset(&n, 0, sizeof n);
    if (!RaClient_Read(SAVE_BASE, n.sv, SAVE_SIZE))
        return false;
    /* of each (current, max) pair the smaller is the current value */
    const int h1 = S16(&n, SV_HP), h2 = S16(&n, SV_HP2), m1 = S16(&n, SV_MP), m2 = S16(&n, SV_MP2);
    n.hp = h1 < h2 ? h1 : h2;
    n.maxHp = h1 < h2 ? h2 : h1;
    n.mp = m1 < m2 ? m1 : m2;
    n.maxMp = m1 < m2 ? m2 : m1;
    n.hearts = S16(&n, SV_HEARTS);
    n.maxHearts = S16(&n, SV_HEARTS + 2);
    n.str = S16(&n, SV_STR);
    n.def = S16(&n, SV_DEF);
    n.intl = S16(&n, SV_INT);
    n.lck = S16(&n, SV_LCK);
    n.level = n.sv[SV_LEVEL];
    n.exp = S32(&n, SV_EXP);
    n.frames = S32(&n, SV_PLAYTIME);
    n.act = n.sv[SV_DSS_ACT] >= 1 && n.sv[SV_DSS_ACT] <= 10 ? n.sv[SV_DSS_ACT] - 1 : -1;
    n.attr = n.sv[SV_DSS_ATTR] >= 1 && n.sv[SV_DSS_ATTR] <= 10 ? n.sv[SV_DSS_ATTR] - 1 : -1;
    n.body = n.sv[SV_EQ_BODY];
    n.arm[0] = n.sv[SV_EQ_ARM_R];
    n.arm[1] = n.sv[SV_EQ_ARM_L];
    n.valid = memcmp(n.sv, "DRACULA AGB", 11) == 0 && n.level >= 1 && n.level <= 99 && n.maxHp > 0 && n.maxHp <= 9999;
    sSt = n;
    return true;
}

static bool WriteU8(uint32_t addr, int v) {
    const uint8_t b = (uint8_t)v;
    return RaClient_Write(addr, &b, 1);
}
static int Count(int id) { return id >= 0 && id <= ITEM_LAST ? sSt.sv[SV_COUNTS + (unsigned)id] : 0; }

/* ------------------------------------------------------------------------------------------------------------------ */
/* drawing + touch helpers                                                                                            */
/* ------------------------------------------------------------------------------------------------------------------ */
#include "ui_common.h"

/* ------------------------------------------------------------------------------------------------------------------ */
/* UI state                                                                                                           */
/* ------------------------------------------------------------------------------------------------------------------ */
enum { TAB_DSS, TAB_EQUIP, TAB_ITEM, TAB_MAGIC, TAB_MAP, TAB_COUNT };
static const char* kTabName[TAB_COUNT] = { "DSS", "EQUIP", "ITEM", "MAGIC", "MAP" };

enum {
    H_TAB = 100,      /* + tab */
    H_SUBTAB = 120,   /* + 0..2 */
    H_ROW = 200,      /* + visible row index */
    H_SCROLL_UP = 90,
    H_SCROLL_DN = 91,
    H_CARD = 300,     /* + 0..19 (action cards, attribute cards) */
    H_CELL = 400      /* + action * 10 + attribute */
};

enum { SUB_BODY, SUB_ARM_R, SUB_ARM_L, SUB_COUNT };
enum { DSS_SET, DSS_COMBOS };

static int sTab = TAB_DSS, sPrevTab = TAB_DSS;
static int sDssView = DSS_SET;
static int sEquipSub = SUB_BODY;
static int sScroll[TAB_COUNT][SUB_COUNT];
static int sSelected = -1; /* item id / magic item shown in the description box */
static int sSelCard = -1;  /* 0..9 action, 10..19 attribute: card whose lore is shown */
static int sSelAct = -1, sSelAttr = -1; /* combination shown in the COMBOS view */
static char sToast[96];
static Uint32 sToastUntil;
static bool sConnected;

static void Toast(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(sToast, sizeof sToast, fmt, ap);
    va_end(ap);
    sToastUntil = SDL_GetTicks() + 2200;
}

typedef struct {
    int id;       /* item id, or -1 for the "unequip" row */
    int count;
    bool equipped;
} Row;
static Row sRows[64];
static int sRowCount;

static bool CardOwned(int idx) { return sSt.sv[SV_CARDS + (unsigned)idx] != 0; }
static bool ComboFound(int a, int b) { return sSt.sv[SV_COMBOS + (unsigned)(a * 10 + b)] != 0; }

static void BuildRows(void) {
    sRowCount = 0;
    if (!sSt.valid)
        return;
    if (sTab == TAB_EQUIP) {
        const int slot = sEquipSub == SUB_BODY ? -1 : sEquipSub - SUB_ARM_R;
        const int cur = slot < 0 ? sSt.body : sSt.arm[slot];
        sRows[sRowCount++] = (Row){ -1, 0, cur == 0 };
        const int lo = slot < 0 ? 1 : ITEM_ARM_FIRST, hi = slot < 0 ? ITEM_ARMOR_LAST : ITEM_ARM_LAST;
        for (int id = lo; id <= hi; id++)
            if (Count(id) > 0 || cur == id)
                sRows[sRowCount++] = (Row){ id, Count(id), cur == id };
    } else if (sTab == TAB_ITEM) {
        for (int id = ITEM_USE_FIRST; id <= ITEM_LAST; id++)
            if (Count(id) > 0)
                sRows[sRowCount++] = (Row){ id, Count(id), false };
    }
}

/* ------------------------------------------------------------------------------------------------------------------ */
/* actions                                                                                                            */
/* ------------------------------------------------------------------------------------------------------------------ */
static void EquipItem(int id) {
    const int slot = sEquipSub == SUB_BODY ? -1 : sEquipSub - SUB_ARM_R;
    const int old = slot < 0 ? sSt.body : sSt.arm[slot];
    const int nid = id < 0 ? 0 : id;
    if (old == nid)
        return;
    bool ok = WriteU8(slot < 0 ? SV_EQ_BODY + SAVE_BASE : SV_EQ_ARM_R + SAVE_BASE + (uint32_t)slot, nid);
    /* like the game: the worn item is not in the stock count */
    if (ok && old > 0 && Count(old) < 99)
        ok = WriteU8(SAVE_BASE + SV_COUNTS + (uint32_t)old, Count(old) + 1);
    if (ok && nid > 0 && Count(nid) > 0)
        ok = WriteU8(SAVE_BASE + SV_COUNTS + (uint32_t)nid, Count(nid) - 1);
    Toast(ok ? (nid ? "Equipped %s" : "Unequipped") : "Write failed", sName[nid]);
    sSelected = nid ? nid : -1;
}

static void SetDss(int act, int attr) {
    bool ok = true;
    if (act >= 0)
        ok = WriteU8(SAVE_BASE + SV_DSS_ACT, act + 1);
    if (ok && attr >= 0)
        ok = WriteU8(SAVE_BASE + SV_DSS_ATTR, attr + 1);
    if (!ok)
        Toast("Write failed");
}

static void ActivateRow(int r) {
    if (r < 0 || r >= sRowCount)
        return;
    if (sTab == TAB_EQUIP)
        EquipItem(sRows[r].id);
    else if (sTab == TAB_ITEM)
        sSelected = sRows[r].id;
}

/* ------------------------------------------------------------------------------------------------------------------ */
/* screens                                                                                                            */
/* ------------------------------------------------------------------------------------------------------------------ */
static void DrawHeader(void) {
    FillRect(0, 0, W, HDR_H, C_PANEL);
    FillRect(0, HDR_H - 2, W, 2, C_GOLD);
    if (!sSt.valid) {
        Text(sFont22, 14, 14, 0, C_GOLD, "NATHAN GRAVES");
        return;
    }
    Text(sFont22, 12, 4, 0, C_GOLD, "NATHAN");
    Text(sFont18, 104, 8, 0, C_CREAM, "Lv %d", sSt.level);
    Text(sFont14, 160, 4, 0, C_RED, "HP");
    Text(sFont14, 184, 4, 0, C_CREAM, "%d/%d", sSt.hp, sSt.maxHp);
    Text(sFont14, 160, 21, 0, C_BLUE, "MP");
    Text(sFont14, 184, 21, 0, C_CREAM, "%d/%d", sSt.mp, sSt.maxMp);
    FillRect(272, 8, 90, 8, C_BG);
    FillRect(272, 8, sSt.maxHp ? (int)(90.0 * (sSt.hp > sSt.maxHp ? sSt.maxHp : sSt.hp) / sSt.maxHp) : 0, 8, C_RED);
    FillRect(272, 25, 90, 8, C_BG);
    FillRect(272, 25, sSt.maxMp ? (int)(90.0 * (sSt.mp > sSt.maxMp ? sSt.maxMp : sSt.mp) / sSt.maxMp) : 0, 8, C_BLUE);
    Text(sFont14, 376, 4, 0, C_GOLD, "STR");
    Text(sFont14, 412, 4, 0, C_CREAM, "%d", sSt.str);
    Text(sFont14, 376, 21, 0, C_GOLD, "DEF");
    Text(sFont14, 412, 21, 0, C_CREAM, "%d", sSt.def);
    Text(sFont14, 466, 4, 0, C_GOLD, "INT");
    Text(sFont14, 500, 4, 0, C_CREAM, "%d", sSt.intl);
    Text(sFont14, 466, 21, 0, C_GOLD, "LCK");
    Text(sFont14, 500, 21, 0, C_CREAM, "%d", sSt.lck);
    Text(sFont12, 556, 6, 0, C_RED, "\xe2\x99\xa5");
    Text(sFont14, 572, 4, 0, C_RED, "%d/%d", sSt.hearts, sSt.maxHearts);
    const uint32_t secs = sSt.frames / 60;
    Text(sFont12, 12, 38, 0, C_DIM, "EXP %u", sSt.exp);
    Text(sFont12, 628, 38, 2, C_DIM, "%u:%02u:%02u", secs / 3600, (secs / 60) % 60, secs % 60);
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

static void DrawSubtabs(const char* const* names, int count, int active) {
    const int sw = (W - 8) / count;
    for (int i = 0; i < count; i++) {
        const bool act = i == active;
        const int x = 4 + i * sw;
        FillRect(x, BODY_Y, sw - 4, 34, act ? C_SEL : C_PANEL2);
        FrameRect(x, BODY_Y, sw - 4, 34, act ? C_GOLD : C_DIM);
        Text(sFont18, x + (sw - 4) / 2, BODY_Y + 6, 1, act ? C_WHITE : C_CREAM, "%s", names[i]);
        AddHit(x, BODY_Y, sw - 4, 34, H_SUBTAB + i);
    }
}

static void ClampScroll(int* sc, int rows, int visible) {
    const int maxs = rows > visible ? rows - visible : 0;
    if (*sc < 0) *sc = 0;
    if (*sc > maxs) *sc = maxs;
}

static void DrawList(int y0, int h, int* scroll) {
    const int visible = h / ROW_H;
    ClampScroll(scroll, sRowCount, visible);
    const int listW = W - 52;
    for (int v = 0; v < visible; v++) {
        const int r = *scroll + v;
        if (r >= sRowCount)
            break;
        const Row* row = &sRows[r];
        const int y = y0 + v * ROW_H;
        const bool sel = row->id >= 0 && row->id == sSelected;
        FillRect(4, y, listW, ROW_H - 3, row->equipped ? C_SEL : (r & 1 ? C_PANEL : C_PANEL2));
        if (sel)
            FrameRect(4, y, listW, ROW_H - 3, C_GOLD);
        if (row->id < 0) {
            Text(sFont18, 16, y + 7, 0, row->equipped ? C_GOLD : C_DIM, "---  (none)");
        } else {
            Text(sFont18, 16, y + 7, 0, row->equipped ? C_GOLD : C_CREAM, "%s", sName[row->id]);
            if (row->count > 1 || (row->count == 1 && !row->equipped))
                Text(sFont18, listW - 12, y + 7, 2, C_DIM, "x%d", row->count);
            if (row->equipped)
                Text(sFont12, listW - (row->count >= 1 ? 60 : 12), y + 12, 2, C_GOLD, "EQUIPPED");
        }
        AddHit(4, y, listW, ROW_H - 3, H_ROW + v);
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
    if (sRowCount > visible) {
        const int maxs = sRowCount - visible;
        const int th = (h - 8) * visible / sRowCount;
        FillRect(W - 50, y0 + 2 + (h - 8 - th) * (*scroll) / (maxs ? maxs : 1), 3, th, C_GOLD);
    }
}

static void DrawDescBox(int y, int h, const char* title, const char* desc) {
    FillRect(4, y, W - 8, h, C_PANEL);
    FrameRect(4, y, W - 8, h, C_GOLD);
    if (title && title[0])
        Text(sFont18, 14, y + 4, 0, C_GOLD, "%s", title);
    if (desc && desc[0])
        Paragraph(sFont14, 14, y + (title && title[0] ? 28 : 8), W - 28, 18, (h - 30) / 18, C_CREAM, desc);
}

/* "STR +10  DEF +500 ..." for an equipment id */
static void StatLine(char* out, size_t cap, int id) {
    static const char* const lab[4] = { "STR", "DEF", "INT", "LCK" };
    out[0] = 0;
    for (int k = 0; id > 0 && id <= ITEM_LAST && k < 4; k++)
        if (sEqStat[id][k])
            snprintf(out + strlen(out), cap - strlen(out), "%s%s %+d", out[0] ? "   " : "", lab[k], sEqStat[id][k]);
}

static const char* ComboText(int a, int b, bool* found) {
    *found = a >= 0 && b >= 0 && ComboFound(a, b);
    if (a < 0 || b < 0)
        return sComboDesc[0];
    return *found ? sComboDesc[1 + a * 10 + b] : "??? - this combination has not been found yet.";
}

static void DrawCardButton(int idx, int x, int y, int w, int h) {
    const bool act = idx < 10;
    const int i = act ? idx : idx - 10;
    const bool eq = act ? sSt.act == i : sSt.attr == i;
    const bool own = CardOwned(idx);
    FillRect(x, y, w, h, eq ? C_RED : (own ? C_PANEL2 : C_BG));
    FrameRect(x, y, w, h, eq ? C_GOLD : (idx == sSelCard ? C_CREAM : C_DIM));
    Text(sFont14, x + w / 2, y + 8, 1, eq ? C_WHITE : (own ? C_CREAM : C_DIM), "%s", own ? (act ? kActName[i] : kAttrName[i]) : "???");
    AddHit(x, y, w, h, H_CARD + idx);
}

static void DrawDss(void) {
    static const char* const names[2] = { "CARDS", "COMBOS" };
    DrawSubtabs(names, 2, sDssView);
    const int top = BODY_Y + 38;
    bool found;
    if (sDssView == DSS_SET) {
        const int cw = 124, ch = 30;
        Text(sFont14, 8, top, 0, C_GOLD, "ACTION");
        for (int i = 0; i < 10; i++)
            DrawCardButton(i, 4 + (i % 5) * (cw + 4), top + 20 + (i / 5) * (ch + 3), cw, ch);
        const int ay = top + 20 + 2 * (ch + 3) + 4;
        Text(sFont14, 8, ay, 0, C_GOLD, "ATTRIBUTE");
        for (int i = 0; i < 10; i++)
            DrawCardButton(10 + i, 4 + (i % 5) * (cw + 4), ay + 20 + (i / 5) * (ch + 3), cw, ch);
        const int dy = ay + 20 + 2 * (ch + 3) + 4;
        char title[96];
        snprintf(title, sizeof title, "%s + %s", sSt.act >= 0 ? kActName[sSt.act] : "---", sSt.attr >= 0 ? kAttrName[sSt.attr] : "---");
        const char* eff = ComboText(sSt.act, sSt.attr, &found);
        DrawDescBox(dy, TAB_Y - 6 - dy, title, eff);
        if (sSelCard >= 0 && sSelCard < 20 && CardOwned(sSelCard)) {
            const int y = dy + 28 + 18 * 2 + 6;
            if (y + 18 < TAB_Y - 8)
                Paragraph(sFont12, 14, y, W - 28, 15, (TAB_Y - 8 - y) / 15, C_DIM, sCardDesc[sSelCard < 10 ? 10 + sSelCard : sSelCard - 10]);
        }
    } else {
        const int lx = 4, lw = 44, cw = 58, ch = 22, y0 = top + 20;
        for (int b = 0; b < 10; b++)
            Text(sFont12, lx + lw + b * cw + cw / 2, top + 2, 1, C_GOLD, "%s", kAttrAbbr[b]);
        for (int a = 0; a < 10; a++) {
            Text(sFont12, lx + 2, y0 + a * ch + 4, 0, C_GOLD, "%s", kActAbbr[a]);
            for (int b = 0; b < 10; b++) {
                const int x = lx + lw + b * cw, y = y0 + a * ch;
                const bool eq = sSt.act == a && sSt.attr == b, sel = sSelAct == a && sSelAttr == b;
                FillRect(x + 1, y + 1, cw - 2, ch - 2, eq ? C_RED : (ComboFound(a, b) ? C_GREEN : C_PANEL2));
                if (eq || sel)
                    FrameRect(x, y, cw, ch, eq ? C_GOLD : C_WHITE);
                if (!ComboFound(a, b))
                    Text(sFont12, x + cw / 2, y + 3, 1, C_DIM, "?");
                AddHit(x, y, cw, ch, H_CELL + a * 10 + b);
            }
        }
        const int dy = y0 + 10 * ch + 4;
        const int a = sSelAct >= 0 ? sSelAct : sSt.act, b = sSelAct >= 0 ? sSelAttr : sSt.attr;
        char title[96];
        snprintf(title, sizeof title, "%s + %s", a >= 0 ? kActName[a] : "---", b >= 0 ? kAttrName[b] : "---");
        DrawDescBox(dy, TAB_Y - 6 - dy, title, ComboText(a, b, &found));
    }
}

static void DrawEquip(void) {
    static const char* const names[SUB_COUNT] = { "BODY", "ARM R", "ARM L" };
    DrawSubtabs(names, SUB_COUNT, sEquipSub);
    const int y = BODY_Y + 38;
    FillRect(4, y, W - 8, 26, C_PANEL);
    const int cur[SUB_COUNT] = { sSt.body, sSt.arm[0], sSt.arm[1] };
    const int cx[SUB_COUNT] = { 12, 224, 436 };
    for (int i = 0; i < SUB_COUNT; i++)
        Text(sFont14, cx[i], y + 4, 0, i == sEquipSub ? C_GOLD : C_CREAM, "%s", cur[i] ? sName[cur[i]] : "---");
    BuildRows();
    DrawList(y + 30, 6 * ROW_H, &sScroll[TAB_EQUIP][sEquipSub]);
    const int dy = y + 30 + 6 * ROW_H + 2;
    const int sel = sSelected > 0 ? sSelected : cur[sEquipSub];
    char stats[96];
    StatLine(stats, sizeof stats, sel);
    char text[240];
    snprintf(text, sizeof text, "%s%s%s", sel > 0 ? sDesc[sel] : "", stats[0] ? "   [" : "", stats[0] ? stats : "");
    if (stats[0])
        strncat(text, "]", sizeof text - strlen(text) - 1);
    DrawDescBox(dy, TAB_Y - 6 - dy, sel > 0 ? sName[sel] : "", text);
}

static void DrawItems(void) {
    BuildRows();
    const int listH = 7 * ROW_H;
    DrawList(BODY_Y, listH, &sScroll[TAB_ITEM][0]);
    if (sRowCount == 0)
        Text(sFont18, 280, BODY_Y + 100, 1, C_DIM, "No items.");
    const int dy = BODY_Y + listH + 2;
    const bool ok = sSelected >= ITEM_USE_FIRST && sSelected <= ITEM_LAST;
    DrawDescBox(dy, TAB_Y - 6 - dy, ok ? sName[sSelected] : "", ok ? sDesc[sSelected] : "Tap an item.");
}

static void DrawMagic(void) {
    Text(sFont14, 14, BODY_Y + 2, 0, C_DIM, "Magic items - tap one for its description");
    for (int i = 0; i < 8; i++) {
        const int x = 4 + (i % 2) * 318, y = BODY_Y + 26 + (i / 2) * (ROW_H + 4);
        const bool own = sSt.sv[SV_MAGIC + (unsigned)i] != 0;
        FillRect(x, y, 312, ROW_H, own ? C_PANEL2 : C_BG);
        FrameRect(x, y, 312, ROW_H, i == sSelected - 1000 ? C_GOLD : C_DIM);
        Text(sFont18, x + 12, y + 8, 0, own ? C_CREAM : C_DIM, "%s", own ? kMagicName[i] : "??????????");
        Text(sFont12, x + 300, y + 12, 2, own ? C_GREEN : C_DIM, own ? "OBTAINED" : "");
        AddHit(x, y, 312, ROW_H, H_ROW + i);
    }
    const int dy = BODY_Y + 26 + 4 * (ROW_H + 4) + 2;
    const int sel = sSelected >= 1000 ? sSelected - 1000 : -1;
    const bool own = sel >= 0 && sSt.sv[SV_MAGIC + (unsigned)sel];
    DrawDescBox(dy, TAB_Y - 6 - dy, own ? kMagicName[sel] : "", own ? sMagicDesc[sel] : "");
}

static void DrawMap(void) {
    const int cell = 12, mx = (W - MAP_COLS * cell) / 2, my = 6;
    FillRect(mx - 2, my - 2, MAP_COLS * cell + 4, MAP_ROWS * cell + 4, C_PANEL);
    int rooms = 0;
    for (int r = 0; r < MAP_ROWS; r++) {
        uint64_t row = 0;
        for (int k = 7; k >= 0; k--)
            row = (row << 8) | sSt.sv[SV_MAP + (unsigned)(r * 8 + k)];
        for (int c = 0; c < MAP_COLS; c++) {
            if (!((row >> c) & 1))
                continue;
            rooms++;
            const int x = mx + c * cell, y = my + r * cell;
            FillRect(x, y, cell, cell, C_BLUE);
            FrameRect(x, y, cell, cell, (Col){ 150, 184, 240 });
        }
    }
    FillRect(4, 6, 118, 24, C_PANEL2);
    FrameRect(4, 6, 118, 24, C_GOLD);
    Text(sFont14, 63, 9, 1, C_WHITE, "%d rooms", rooms);
}

static void DrawStatus(const char* l1, const char* l2) {
    FillRect(0, 0, W, H, C_BG);
    Text(sFont22, W / 2, 170, 1, C_GOLD, "CIRCLE OF THE MOON");
    Text(sFont18, W / 2, 214, 1, C_CREAM, "%s", l1);
    Text(sFont14, W / 2, 246, 1, C_DIM, "%s", l2);
}

/* SELECT on the handheld toggles the map tab. */
static int sPadFd[4], sPadCount;
static void OpenPads(void) {
    const char* forced = getenv("COTM_PAD_DEV");
    if (forced) {
        const int fd = open(forced, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd >= 0)
            sPadFd[sPadCount++] = fd;
        fprintf(stderr, "[cotm] pad: %s (%s)\n", forced, fd >= 0 ? "ok" : "cannot open");
        return;
    }
    for (int i = 0; i < 32 && sPadCount < 4; i++) {
        char sysfs[128], name[128] = "", path[64];
        snprintf(sysfs, sizeof sysfs, "/sys/class/input/event%d/device/name", i);
        FILE* f = fopen(sysfs, "r");
        if (!f)
            continue;
        const bool got = fgets(name, sizeof name, f) != NULL;
        fclose(f);
        name[strcspn(name, "\r\n")] = 0;
        if (!got || !(strstr(name, "joypad") || strstr(name, "gamepad") || strstr(name, "Gamepad")))
            continue;
        snprintf(path, sizeof path, "/dev/input/event%d", i);
        const int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        fprintf(stderr, "[cotm] pad: %s %s (%s)\n", path, name, fd >= 0 ? "ok" : "cannot open");
        if (fd >= 0)
            sPadFd[sPadCount++] = fd;
    }
}

static bool PumpPads(void) {
    bool select = false;
    struct input_event ev[16];
    for (int p = 0; p < sPadCount; p++) {
        ssize_t n;
        while ((n = read(sPadFd[p], ev, sizeof ev)) > 0)
            for (size_t i = 0; i < (size_t)n / sizeof ev[0]; i++)
                if (ev[i].type == EV_KEY && ev[i].code == BTN_SELECT && ev[i].value == 1)
                    select = true;
    }
    return select;
}

static void OnSelect(void) {
    fprintf(stderr, "[cotm] SELECT pressed\n");
    if (sTab == TAB_MAP) {
        sTab = sPrevTab;
    } else {
        sPrevTab = sTab;
        sTab = TAB_MAP;
    }
}

static void DrawFrame(void) {
    SDL_SetRenderDrawColor(sRen, C_BG.r, C_BG.g, C_BG.b, 255);
    SDL_RenderClear(sRen);
    sHitCount = 0;
    if (!sConnected) {
        DrawStatus("Waiting for RetroArch...", "Launch Castlevania: Circle of the Moon (mGBA core)");
    } else if (!sSt.valid) {
        DrawStatus("Load a saved game", "Choose a file on the title screen to begin");
    } else {
        if (sTab != TAB_MAP)
            DrawHeader();
        switch (sTab) {
            case TAB_DSS: DrawDss(); break;
            case TAB_EQUIP: DrawEquip(); break;
            case TAB_ITEM: DrawItems(); break;
            case TAB_MAGIC: DrawMagic(); break;
            case TAB_MAP: DrawMap(); break;
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

/* ------------------------------------------------------------------------------------------------------------------ */
/* input dispatch                                                                                                     */
/* ------------------------------------------------------------------------------------------------------------------ */
static int ListArea(int* y0, int* h) {
    switch (sTab) {
        case TAB_EQUIP: *y0 = BODY_Y + 38 + 30; *h = 6 * ROW_H; return 1;
        case TAB_ITEM: *y0 = BODY_Y; *h = 7 * ROW_H; return 1;
        default: return 0;
    }
}
static int* CurScroll(void) { return &sScroll[sTab][sTab == TAB_EQUIP ? sEquipSub : 0]; }

static void OnTap(int x, int y) {
    const int id = HitTest(x, y);
    if (id < 0)
        return;
    if (id >= H_TAB && id < H_TAB + TAB_COUNT) {
        sTab = id - H_TAB;
        if (sTab != TAB_MAP)
            sPrevTab = sTab;
        sSelected = -1;
    } else if (id >= H_SUBTAB && id < H_SUBTAB + SUB_COUNT) {
        if (sTab == TAB_DSS)
            sDssView = id - H_SUBTAB;
        else
            sEquipSub = id - H_SUBTAB;
        sSelected = -1;
    } else if (id >= H_ROW && id < H_ROW + 40) {
        if (sTab == TAB_MAGIC)
            sSelected = 1000 + (id - H_ROW);
        else {
            BuildRows();
            ActivateRow(*CurScroll() + (id - H_ROW));
        }
    } else if (id == H_SCROLL_UP) {
        *CurScroll() -= 3;
    } else if (id == H_SCROLL_DN) {
        *CurScroll() += 3;
    } else if (id >= H_CARD && id < H_CARD + 20) {
        const int c = id - H_CARD;
        sSelCard = c;
        if (CardOwned(c)) {
            SetDss(c < 10 ? c : -1, c >= 10 ? c - 10 : -1);
            Toast("%s", c < 10 ? kActName[c] : kAttrName[c - 10]);
        } else {
            Toast("Card not found yet");
        }
    } else if (id >= H_CELL && id < H_CELL + 100) {
        const int a = (id - H_CELL) / 10, b = (id - H_CELL) % 10;
        sSelAct = a;
        sSelAttr = b;
        if (CardOwned(a) && CardOwned(10 + b))
            SetDss(a, b);
        else
            Toast("Card not found yet");
    }
    fprintf(stderr, "[cotm] tap (%d,%d) -> %d\n", x, y, id);
    if (sConnected)
        PollState();
}

static void ApplyDrag(void) {
    int y0, h;
    if (abs(sDragDy) < ROW_H / 2 || !ListArea(&y0, &h))
        return;
    if (sTy >= y0 && sTy < y0 + h) {
        const int rows = sDragDy / ROW_H ? sDragDy / ROW_H : (sDragDy > 0 ? 1 : -1);
        *CurScroll() -= rows;
    }
    sDragDy = 0;
}

/* ------------------------------------------------------------------------------------------------------------------ */
int main(int argc, char** argv) {
    (void)argc;
    char dir[512] = ".";
    if (strrchr(argv[0], '/')) {
        snprintf(dir, sizeof dir, "%s", argv[0]);
        *strrchr(dir, '/') = 0;
    }
    if (getenv("COTM_DATA_DIR"))
        snprintf(dir, sizeof dir, "%s", getenv("COTM_DATA_DIR"));
    InitCharTable();
    if (!RaClient_Open(getenv("COTM_HOST") ? getenv("COTM_HOST") : "127.0.0.1", 55355)) {
        fprintf(stderr, "[cotm] cannot open the RetroArch command socket\n");
        return 1;
    }
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0 || TTF_Init() != 0) {
        fprintf(stderr, "[cotm] SDL init failed: %s\n", SDL_GetError());
        return 1;
    }
    const int displays = SDL_GetNumVideoDisplays();
    const bool windowed = getenv("COTM_WINDOWED") != NULL || displays < 2;
    Uint32 flags = SDL_WINDOW_SHOWN;
    int px = SDL_WINDOWPOS_UNDEFINED, py = SDL_WINDOWPOS_UNDEFINED;
    if (!windowed) {
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_BORDERLESS;
        px = SDL_WINDOWPOS_UNDEFINED_DISPLAY(1);
        py = SDL_WINDOWPOS_UNDEFINED_DISPLAY(1);
    }
    /* the sway config moves windows titled "...Bottom Screen" to DSI-1 */
    sWin = SDL_CreateWindow("Circle of the Moon - Bottom Screen", px, py, W, H, flags);
    if (!sWin) {
        fprintf(stderr, "[cotm] window failed: %s\n", SDL_GetError());
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
        fprintf(stderr, "[cotm] cannot load font %s: %s\n", fontPath, TTF_GetError());
        return 1;
    }
    sTouchFd = OpenBottomTouch();
    OpenPads();

    /* QA: COTM_TEST_TAPS="ms,x,y;ms,x,y" injects taps through the real tap handler */
    struct { unsigned ms; int x, y; bool done; } taps[32];
    int tapCount = 0;
    if (getenv("COTM_TEST_TAPS")) {
        char* spec = strdup(getenv("COTM_TEST_TAPS"));
        for (char* tok = strtok(spec, ";"); tok && tapCount < 32; tok = strtok(NULL, ";")) {
            unsigned ms;
            int x, y;
            if (sscanf(tok, "%u,%d,%d", &ms, &x, &y) == 3)
                taps[tapCount++] = (typeof(taps[0])){ ms, x, y, false };
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
            if (e.type == SDL_QUIT)
                running = false;
            else if (e.type == SDL_FINGERDOWN && sTouchFd < 0)
                OnTap((int)(e.tfinger.x * W), (int)(e.tfinger.y * H));
            else if (e.type == SDL_MOUSEBUTTONDOWN && sTouchFd < 0)
                OnTap(e.button.x, e.button.y);
            else if (e.type == SDL_MOUSEWHEEL)
                *CurScroll() -= e.wheel.y;
        }
        PumpTouch();
        if (PumpPads())
            OnSelect();
        if (sPendingTap) {
            sPendingTap = false;
            OnTap(sTapX, sTapY);
        }
        ApplyDrag();
        for (int i = 0; i < tapCount; i++)
            if (!taps[i].done && SDL_GetTicks() - startMs >= taps[i].ms) {
                taps[i].done = true;
                fprintf(stderr, "[cotm] test tap (%d,%d)\n", taps[i].x, taps[i].y);
                OnTap(taps[i].x, taps[i].y);
            }

        const Uint32 now = SDL_GetTicks();
        if (now - lastPoll >= 120) {
            lastPoll = now;
            if (PollState()) {
                if (!sConnected)
                    fprintf(stderr, "[cotm] connected to RetroArch\n");
                sConnected = true;
                silent = 0;
                if (!sRomLoaded && now - lastRomTry > 1000) {
                    lastRomTry = now;
                    uint8_t code[4] = { 0 };
                    if (RaClient_Read(COTM_ROM_CODE_ADDR, code, 4) && memcmp(code, "AAME", 4) == 0 && LoadRom()) {
                        sRomLoaded = true;
                        fprintf(stderr, "[cotm] ROM data loaded (%s / %s)\n", sName[1], sComboDesc[1]);
                    } else if (code[0] && memcmp(code, "AAME", 4) != 0) {
                        fprintf(stderr, "[cotm] not Circle of the Moon (USA): %.4s\n", code);
                    }
                }
            } else if (++silent > 8u * (sConnected ? MAX_SILENT_SECONDS : STARTUP_GRACE_SECONDS)) {
                fprintf(stderr, "[cotm] RetroArch is gone, exiting\n");
                break;
            }
        }
        if (sRomLoaded || !sConnected) {
            DrawFrame();
        } else {
            sHitCount = 0;
            SDL_SetRenderDrawColor(sRen, C_BG.r, C_BG.g, C_BG.b, 255);
            SDL_RenderClear(sRen);
            DrawStatus("Reading game data...", "");
            SDL_RenderPresent(sRen);
        }
        SDL_Delay(16);
    }
    RaClient_Close();
    if (sTouchFd >= 0)
        close(sTouchFd);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
