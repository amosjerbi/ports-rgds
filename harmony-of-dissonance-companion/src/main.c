/* hod-bottom: touch companion for Castlevania: Harmony of Dissonance running in RetroArch's mGBA core on the RG DS.
 * Runs on the bottom panel (DSI-1). Reads the game's RAM and ROM over RetroArch's network command interface
 * (READ_CORE_MEMORY, UDP 55355) and writes equipment / spell books / relic switches back (WRITE_CORE_MEMORY).
 * Tabs: SPELLBOOK, EQUIP, ITEM, RELICS, ENEMY. It never touches the emulator's video or input. */
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hod_data.h"
#include "ra_client.h"

#define W 640
#define H 480
#define HDR_H 58
#define TAB_Y 424
#define TAB_H 56
#define BODY_Y (HDR_H + 4)
#define BODY_H (TAB_Y - BODY_Y - 4)
#define ROW_H 38
#define MAX_SILENT_SECONDS 20
#define STARTUP_GRACE_SECONDS 180

/* ------------------------------------------------------------------------------------------------------------------ */
/* game data                                                                                                          */
/* ------------------------------------------------------------------------------------------------------------------ */
typedef struct {
    uint16_t hp, exp;
    uint8_t level, tol, weak;
    uint8_t drop[2][2]; /* (id, type) x2 */
} Enemy;

static char sStr[STR_LAST + 1][112];
static Enemy sEnemy[ENEMY_COUNT];
static bool sRomLoaded;

typedef struct {
    uint8_t blk[RAM_BLOCK_SIZE];
    bool valid;
    int level, hp, mp, maxHp, maxMp, hearts, maxHearts, str, def, intl, lck;
    uint32_t exp, gold, playFrames;
    uint8_t consumables[28];
    uint8_t equipCount[EQUIP_ID_COUNT];
    int eqBody, eqAcc[3], eqWhip;   /* item name index or -1 */
    uint8_t books, bookSel;
    uint16_t relicsOwned, relicsOn;
    uint8_t defeated[13];
} State;
static State sSt;

#define BLK(addr) (sSt.blk[(addr) - RAM_BLOCK_BASE])

static const char* kBookName[5] = { "Fire Book", "Ice Book", "Bolt Book", "Wind Book", "Summoning Tome" };
static const int kBookNameIdx[5] = { 225, 226, 227, 228, 229 };
static const int kRelicNameIdx[12] = { 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241 };

/* ------------------------------------------------------------------------------------------------------------------ */
/* ROM text / tables                                                                                                  */
/* ------------------------------------------------------------------------------------------------------------------ */
/* Text is u16 codes: 8260.. = A-Z, 8548.. = a-z, 824F.. = 0-9, 8540 = space, F006 = line break, F00A/F007 = end. */
static void DecodeText(char* dst, size_t cap, const uint8_t* src, size_t n) {
    size_t o = 0, i = 0;
    while (i + 1 < n && src[i] == 0 && src[i + 1] == 0) /* leading 0000 header words */
        i += 2;
    for (; i + 1 < n && o + 4 < cap; i += 2) {
        const unsigned v = (unsigned)(src[i] | (src[i + 1] << 8));
        if (v == 0xF00A || v == 0xF007)
            break;
        char c = 0;
        if (v >= 0x8260 && v <= 0x8279) c = (char)('A' + v - 0x8260);
        else if (v >= 0x8548 && v <= 0x8561) c = (char)('a' + v - 0x8548);
        else if (v >= 0x824F && v <= 0x8258) c = (char)('0' + v - 0x824F);
        else switch (v) {
            case 0x8540: c = ' '; break;
            case 0x8547: c = 'I'; break;
            case 0x8541: c = ','; break;
            case 0x8542: c = '.'; break;
            case 0x8562: c = '\''; break;
            case 0x817C: c = '-'; break;
            case 0x8565: c = '$'; break;
            case 0x8148: c = '?'; break;
            case 0x8149: case 0x8546: c = '!'; break;
            case 0x8168: c = '"'; break;
            case 0x8563: c = '('; break;
            case 0x8564: c = ')'; break;
            case 0x815E: c = '/'; break;
            case 0xF006: c = '\n'; break;
            default: break;
        }
        if (c == '\n') {
            if (o && dst[o - 1] != ' ')
                dst[o++] = ' ';
        } else if (c) {
            dst[o++] = c;
        }
    }
    while (o && dst[o - 1] == ' ')
        o--;
    dst[o] = 0;
}

static bool LoadRom(void) {
    static uint32_t ptrs[STR_LAST + 1];
    const size_t textLen = ROM_TEXT_HI - ROM_TEXT_LO + 64;
    uint8_t* text = malloc(textLen);
    uint8_t* et = malloc(ENEMY_COUNT * ENEMY_REC + 4);
    bool ok = text && et;
    ok = ok && RaClient_Read(ROM_STRING_PTRS + 4u * STR_FIRST, &ptrs[STR_FIRST], 4u * (STR_LAST - STR_FIRST + 1));
    ok = ok && RaClient_Read(ROM_TEXT_LO, text, textLen);
    ok = ok && RaClient_Read(ROM_ENEMY_TABLE - 4, et, ENEMY_COUNT * ENEMY_REC + 4);
    if (ok) {
        for (int i = STR_FIRST; i <= STR_LAST; i++) {
            const uint32_t off = ptrs[i] - ROM_TEXT_LO;
            if (ptrs[i] < ROM_TEXT_LO || off >= textLen) {
                sStr[i][0] = 0;
                continue;
            }
            DecodeText(sStr[i], sizeof sStr[i], text + off, textLen - off > 240 ? 240 : textLen - off);
        }
        for (int e = 0; e < ENEMY_COUNT; e++) {
            const uint8_t* r = et + 4 + e * ENEMY_REC; /* record starts at HP; the drops are the previous record's tail */
            const uint8_t* d = et + e * ENEMY_REC;
            sEnemy[e].hp = (uint16_t)(r[0] | (r[1] << 8));
            sEnemy[e].exp = (uint16_t)(r[2] | (r[3] << 8));
            sEnemy[e].level = r[4];
            sEnemy[e].tol = r[20];
            sEnemy[e].weak = r[21];
            sEnemy[e].drop[0][0] = d[0];
            sEnemy[e].drop[0][1] = d[1];
            sEnemy[e].drop[1][0] = d[2];
            sEnemy[e].drop[1][1] = d[3];
        }
    }
    free(text);
    free(et);
    return ok;
}

static const char* ItemName(int n) { return (n >= STR_ITEM_NAME && n <= 272) ? sStr[n] : "---"; }
static const char* ItemDesc(int n) { return (n >= STR_ITEM_NAME && n <= 272) ? sStr[n + 213] : ""; }

/* what a drop (id, type) is called: type 1 gold, 3 consumable/key, 5 equipment */
static void DropName(char* out, size_t cap, int id, int type) {
    if (type == 1 && id <= 6) snprintf(out, cap, "%s", sStr[STR_GOLD + id]);
    else if (type == 3) snprintf(out, cap, "%s", ItemName(60 + id));
    else if (type == 5) snprintf(out, cap, "%s", ItemName(97 + id));
    else if (type == 0) snprintf(out, cap, "---");
    else snprintf(out, cap, "?");
}

/* ------------------------------------------------------------------------------------------------------------------ */
/* live state                                                                                                         */
/* ------------------------------------------------------------------------------------------------------------------ */
static uint16_t B16(const State* s, uint32_t a) { return (uint16_t)(s->blk[a - RAM_BLOCK_BASE] | (s->blk[a - RAM_BLOCK_BASE + 1] << 8)); }
static uint32_t B32(const State* s, uint32_t a) { return (uint32_t)B16(s, a) | ((uint32_t)B16(s, a + 2) << 16); }

static bool PollState(void) {
    State n;
    memset(&n, 0, sizeof n);
    uint8_t tot[12];
    if (!RaClient_Read(RAM_BLOCK_BASE, n.blk, RAM_BLOCK_SIZE) || !RaClient_Read(RAM_MAXHP, tot, sizeof tot))
        return false;
    if (!RaClient_Read(RAM_PLAYTIME, &n.playFrames, 4))
        n.playFrames = 0;
    n.level = n.blk[RAM_LEVEL - RAM_BLOCK_BASE];
    n.hp = B16(&n, RAM_HP);
    n.mp = B16(&n, RAM_MP);
    n.hearts = B16(&n, RAM_HEARTS);
    n.maxHearts = B16(&n, RAM_MAXHEARTS);
    n.maxHp = tot[0] | (tot[1] << 8);
    n.maxMp = tot[2] | (tot[3] << 8);
    n.str = tot[4] | (tot[5] << 8);
    n.def = tot[6] | (tot[7] << 8);
    n.intl = tot[8] | (tot[9] << 8);
    n.lck = tot[10] | (tot[11] << 8);
    n.exp = B32(&n, RAM_EXP);
    n.gold = B32(&n, RAM_GOLD);
    memcpy(n.consumables, n.blk + (RAM_CONSUMABLES - RAM_BLOCK_BASE), sizeof n.consumables);
    memcpy(n.equipCount, n.blk + (RAM_EQUIPCOUNTS - RAM_BLOCK_BASE), EQUIP_ID_COUNT);
    const int body = n.blk[RAM_EQ_BODY - RAM_BLOCK_BASE];
    n.eqBody = body < EQUIP_ID_COUNT ? 97 + body : -1;
    for (int i = 0; i < 3; i++) {
        const int v = n.blk[RAM_EQ_ACC1 - RAM_BLOCK_BASE + (uint32_t)i];
        n.eqAcc[i] = v < EQUIP_ID_COUNT ? 97 + v : -1;
    }
    const int wh = B16(&n, RAM_EQ_WHIP);
    n.eqWhip = wh <= 8 ? 88 + wh : -1;
    n.books = n.blk[RAM_BOOKS_OWNED - RAM_BLOCK_BASE];
    n.bookSel = n.blk[RAM_SPELLBOOK - RAM_BLOCK_BASE];
    n.relicsOwned = (uint16_t)(n.blk[RAM_RELICS_OWNED - RAM_BLOCK_BASE] | ((n.blk[RAM_RELICS_OWNED + 1 - RAM_BLOCK_BASE] & 15) << 8));
    n.relicsOn = (uint16_t)(n.blk[RAM_RELICS_ON - RAM_BLOCK_BASE] | ((n.blk[RAM_RELICS_ON + 1 - RAM_BLOCK_BASE] & 15) << 8));
    memcpy(n.defeated, n.blk + (RAM_DEFEATED - RAM_BLOCK_BASE), sizeof n.defeated);
    n.valid = n.level >= 1 && n.level <= 99 && n.maxHp > 0 && n.maxHp <= 9999;
    sSt = n;
    return true;
}

static bool WriteBytes(uint32_t addr, const void* data, size_t n) { return RaClient_Write(addr, data, n); }
static bool WriteU8(uint32_t addr, int v) {
    const uint8_t b = (uint8_t)v;
    return WriteBytes(addr, &b, 1);
}
static bool WriteU16(uint32_t addr, int v) {
    const uint8_t b[2] = { (uint8_t)(v & 255), (uint8_t)((v >> 8) & 255) };
    return WriteBytes(addr, b, 2);
}

/* ------------------------------------------------------------------------------------------------------------------ */
/* drawing + touch helpers                                                                                            */
/* ------------------------------------------------------------------------------------------------------------------ */
#include "ui_common.h"

/* ------------------------------------------------------------------------------------------------------------------ */
/* UI state                                                                                                           */
/* ------------------------------------------------------------------------------------------------------------------ */
enum { TAB_BOOK, TAB_EQUIP, TAB_ITEM, TAB_RELIC, TAB_ENEMY, TAB_COUNT };
static const char* kTabName[TAB_COUNT] = { "SPELLBOOK", "EQUIP", "ITEM", "RELICS", "ENEMY" };

enum {
    H_TAB = 100,       /* + tab */
    H_SUBTAB = 120,    /* + 0..4 */
    H_ROW = 200,       /* + visible row index */
    H_SCROLL_UP = 90,
    H_SCROLL_DN = 91,
    H_PAGE_PREV = 93,
    H_PAGE_NEXT = 94,
    H_BACK = 95,
    H_ENEMY_CELL = 300 /* + enemy index */
};

enum { SUB_WHIP, SUB_BODY, SUB_A1, SUB_A2, SUB_A3, SUB_COUNT };

static int sTab = TAB_BOOK;
static int sSub[TAB_COUNT];
static int sScroll[TAB_COUNT][SUB_COUNT];
static int sSelected = -1; /* item name index / book / relic shown in the description box */
static int sEnemyPage, sEnemyDetail = -1;
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
    int id;       /* item name index, book/relic index, or -1 for the "none" row */
    int count;
    bool equipped;
    bool on;
} Row;
static Row sRows[200];
static int sRowCount;

/* equipment type of an item (ROM item table byte +4, low 3 bits): only goods (0) may be worn twice */
static int sEquipType[EQUIP_ID_COUNT];
static int sEquipStat[EQUIP_ID_COUNT][4]; /* STR DEF INT LCK bonus (ROM item table bytes +6..+9) */
static bool sEquipTypeLoaded;

static bool LoadEquipTypes(void) {
    uint8_t t[12 * EQUIP_ID_COUNT];
    if (!RaClient_Read(0x084B2660u, t, sizeof t))
        return false;
    for (int i = 0; i < EQUIP_ID_COUNT; i++) {
        sEquipType[i] = t[12 * i + 4] & 7;
        for (int k = 0; k < 4; k++)
            sEquipStat[i][k] = t[12 * i + 6 + k];
    }
    sEquipTypeLoaded = true;
    return true;
}

/* The game only recomputes STR/DEF/INT/LCK (and moves worn items out of the stock count) when an item is equipped
 * from its own menu, so apply the difference between the old and the new loadout ourselves. */
static void AdjustLoadout(int oldBody, const int oldAcc[3], int newBody, const int newAcc[3]) {
    if (!sEquipTypeLoaded && !LoadEquipTypes())
        return;
    int delta[4] = { 0, 0, 0, 0 };
    const int olds[4] = { oldBody, oldAcc[0], oldAcc[1], oldAcc[2] };
    const int news[4] = { newBody, newAcc[0], newAcc[1], newAcc[2] };
    for (int i = 0; i < 4; i++) {
        if (olds[i] >= 97 && olds[i] - 97 < EQUIP_ID_COUNT)
            for (int k = 0; k < 4; k++) delta[k] -= sEquipStat[olds[i] - 97][k];
        if (news[i] >= 97 && news[i] - 97 < EQUIP_ID_COUNT)
            for (int k = 0; k < 4; k++) delta[k] += sEquipStat[news[i] - 97][k];
    }
    /* the game keeps worn items out of the stock count: put the old item back, take the new one out */
    int counts[EQUIP_ID_COUNT];
    for (int i = 0; i < EQUIP_ID_COUNT; i++)
        counts[i] = sSt.equipCount[i];
    for (int i = 0; i < 4; i++) {
        if (olds[i] == news[i])
            continue;
        if (olds[i] >= 97 && olds[i] - 97 < EQUIP_ID_COUNT && counts[olds[i] - 97] < 99)
            counts[olds[i] - 97]++;
        if (news[i] >= 97 && news[i] - 97 < EQUIP_ID_COUNT && counts[news[i] - 97] > 0)
            counts[news[i] - 97]--;
    }
    for (int i = 0; i < EQUIP_ID_COUNT; i++)
        if (counts[i] != sSt.equipCount[i])
            WriteU8(RAM_EQUIPCOUNTS + (uint32_t)i, counts[i]);
    const int cur[4] = { sSt.str, sSt.def, sSt.intl, sSt.lck };
    for (int k = 0; k < 4; k++) {
        int v = cur[k] + delta[k];
        if (v < 0) v = 0;
        WriteU16(RAM_TOTALS + 2u * (uint32_t)k, v);
    }
}

static void BuildRows(void) {
    sRowCount = 0;
    if (!sSt.valid)
        return;
    if (sTab == TAB_BOOK) {
        for (int i = 0; i < 5; i++)
            if (sSt.books & (1 << i))
                sRows[sRowCount++] = (Row){ i, 0, (sSt.bookSel & 0x7F) == (1 << i), false };
    } else if (sTab == TAB_EQUIP) {
        const int c = sSub[TAB_EQUIP];
        if (c == SUB_WHIP) {
            sRows[sRowCount++] = (Row){ -1, 0, sSt.eqWhip < 0, false };
            for (int n = 88; n <= 96; n++)
                sRows[sRowCount++] = (Row){ n, 0, sSt.eqWhip == n, false };
        } else if (c == SUB_BODY) {
            sRows[sRowCount++] = (Row){ -1, 0, sSt.eqBody < 0, false };
            for (int id = 0; id <= 41; id++)
                if (sSt.equipCount[id] > 0 || sSt.eqBody == 97 + id)
                    sRows[sRowCount++] = (Row){ 97 + id, sSt.equipCount[id], sSt.eqBody == 97 + id, false };
        } else {
            const int slot = c - SUB_A1;
            sRows[sRowCount++] = (Row){ -1, 0, sSt.eqAcc[slot] < 0, false };
            for (int id = 42; id < EQUIP_ID_COUNT; id++) {
                const bool worn = sSt.eqAcc[0] == 97 + id || sSt.eqAcc[1] == 97 + id || sSt.eqAcc[2] == 97 + id;
                if (sSt.equipCount[id] > 0 || worn)
                    sRows[sRowCount++] = (Row){ 97 + id, sSt.equipCount[id], sSt.eqAcc[slot] == 97 + id, false };
            }
        }
    } else if (sTab == TAB_ITEM) {
        for (int i = 0; i < 28; i++)
            if (sSt.consumables[i] > 0)
                sRows[sRowCount++] = (Row){ 60 + i, sSt.consumables[i], false, false };
    } else if (sTab == TAB_RELIC) {
        for (int i = 0; i < 12; i++)
            sRows[sRowCount++] = (Row){ ((sSt.relicsOwned >> i) & 1) ? kRelicNameIdx[i] : -2, 0, false, (sSt.relicsOn >> i) & 1 };
    }
}

/* ------------------------------------------------------------------------------------------------------------------ */
/* actions                                                                                                            */
/* ------------------------------------------------------------------------------------------------------------------ */
static void ActivateRow(int r) {
    if (r < 0 || r >= sRowCount)
        return;
    const Row* row = &sRows[r];
    bool ok = true;
    if (sTab == TAB_BOOK) {
        ok = WriteU8(RAM_SPELLBOOK, (sSt.bookSel & 0x80) | (1 << row->id));
        Toast(ok ? "Selected %s" : "Write failed", kBookName[row->id]);
        sSelected = row->id;
    } else if (sTab == TAB_EQUIP) {
        const int c = sSub[TAB_EQUIP];
        if (c == SUB_WHIP) {
            ok = WriteU16(RAM_EQ_WHIP, row->id < 0 ? 0xFF : row->id - 88);
        } else if (c == SUB_BODY) {
            ok = WriteU8(RAM_EQ_BODY, row->id < 0 ? 0xFF : row->id - 97);
            if (ok)
                AdjustLoadout(sSt.eqBody, sSt.eqAcc, row->id, sSt.eqAcc);
        } else {
            const int slot = c - SUB_A1;
            int newAcc[3] = { sSt.eqAcc[0], sSt.eqAcc[1], sSt.eqAcc[2] };
            newAcc[slot] = row->id;
            if (row->id >= 0) {
                if (!sEquipTypeLoaded)
                    LoadEquipTypes();
                const int id = row->id - 97, type = sEquipTypeLoaded ? sEquipType[id] : 0;
                /* like the game: a head / hands / body type can only be worn once, goods (0) may repeat */
                for (int o = 0; o < 3; o++) {
                    if (o == slot || sSt.eqAcc[o] < 0)
                        continue;
                    const int oid = sSt.eqAcc[o] - 97;
                    if (oid == id || (type != 0 && sEquipTypeLoaded && sEquipType[oid] == type)) {
                        ok = ok && WriteU8(RAM_EQ_ACC1 + (uint32_t)o, 0xFF);
                        newAcc[o] = -1;
                    }
                }
            }
            ok = ok && WriteU8(RAM_EQ_ACC1 + (uint32_t)slot, row->id < 0 ? 0xFF : row->id - 97);
            if (ok)
                AdjustLoadout(sSt.eqBody, sSt.eqAcc, sSt.eqBody, newAcc);
        }
        Toast(ok ? (row->id < 0 ? "Unequipped" : "Equipped %s") : "Write failed", ItemName(row->id));
        sSelected = row->id;
    } else if (sTab == TAB_ITEM) {
        sSelected = row->id;
    } else if (sTab == TAB_RELIC) {
        if (row->id >= 0) {
            const int bit = r;
            const uint16_t on = sSt.relicsOn ^ (uint16_t)(1 << bit);
            const uint8_t hi = (uint8_t)((sSt.blk[RAM_RELICS_ON + 1 - RAM_BLOCK_BASE] & 0xF0) | ((on >> 8) & 15));
            ok = WriteU8(RAM_RELICS_ON, on & 255) && WriteU8(RAM_RELICS_ON + 1, hi);
            Toast(ok ? "%s %s" : "Write failed", ItemName(row->id), row->on ? "OFF" : "ON");
            sSelected = row->id;
        }
    }
}

/* ------------------------------------------------------------------------------------------------------------------ */
/* screens                                                                                                            */
/* ------------------------------------------------------------------------------------------------------------------ */
static void DrawHeader(void) {
    FillRect(0, 0, W, HDR_H, C_PANEL);
    FillRect(0, HDR_H - 2, W, 2, C_GOLD);
    if (!sSt.valid) {
        Text(sFont22, 14, 14, 0, C_GOLD, "JUSTE BELMONT");
        return;
    }
    Text(sFont22, 12, 4, 0, C_GOLD, "JUSTE");
    Text(sFont18, 92, 8, 0, C_CREAM, "Lv %d", sSt.level);
    Text(sFont14, 156, 4, 0, C_RED, "HP");
    Text(sFont14, 180, 4, 0, C_CREAM, "%d/%d", sSt.hp, sSt.maxHp);
    Text(sFont14, 156, 21, 0, C_BLUE, "MP");
    Text(sFont14, 180, 21, 0, C_CREAM, "%d/%d", sSt.mp, sSt.maxMp);
    FillRect(262, 8, 98, 8, C_BG);
    FillRect(262, 8, sSt.maxHp ? (int)(98.0 * (sSt.hp > sSt.maxHp ? sSt.maxHp : sSt.hp) / sSt.maxHp) : 0, 8, C_RED);
    FillRect(262, 25, 98, 8, C_BG);
    FillRect(262, 25, sSt.maxMp ? (int)(98.0 * (sSt.mp > sSt.maxMp ? sSt.maxMp : sSt.mp) / sSt.maxMp) : 0, 8, C_BLUE);
    Text(sFont14, 372, 4, 0, C_GOLD, "STR");
    Text(sFont14, 408, 4, 0, C_CREAM, "%d", sSt.str);
    Text(sFont14, 372, 21, 0, C_GOLD, "DEF");
    Text(sFont14, 408, 21, 0, C_CREAM, "%d", sSt.def);
    Text(sFont14, 456, 4, 0, C_GOLD, "INT");
    Text(sFont14, 490, 4, 0, C_CREAM, "%d", sSt.intl);
    Text(sFont14, 456, 21, 0, C_GOLD, "LCK");
    Text(sFont14, 490, 21, 0, C_CREAM, "%d", sSt.lck);
    Text(sFont14, 540, 4, 0, C_RED, "%d/%d", sSt.hearts, sSt.maxHearts);
    Text(sFont12, 524, 6, 0, C_RED, "\xe2\x99\xa5");
    const uint32_t secs = sSt.playFrames / 60;
    Text(sFont12, 12, 38, 0, C_DIM, "EXP %u", sSt.exp);
    Text(sFont12, 130, 38, 0, C_YELLOW, "%u G", sSt.gold);
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

static int ClampScroll(int* sc, int rows, int visible) {
    const int maxs = rows > visible ? rows - visible : 0;
    if (*sc < 0) *sc = 0;
    if (*sc > maxs) *sc = maxs;
    return maxs;
}

static const char* RowName(const Row* row) {
    if (sTab == TAB_BOOK) return kBookName[row->id];
    return ItemName(row->id);
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
        if (row->id == -1) {
            Text(sFont18, 16, y + 7, 0, row->equipped ? C_GOLD : C_DIM, "---  (none)");
        } else if (row->id == -2) {
            Text(sFont18, 16, y + 7, 0, C_DIM, "??????????");
        } else {
            Text(sFont18, 16, y + 7, 0, row->equipped ? C_GOLD : C_CREAM, "%s", RowName(row));
            if (sTab == TAB_RELIC) {
                FillRect(listW - 56, y + 6, 52, ROW_H - 15, row->on ? C_BLUE : C_RED);
                Text(sFont14, listW - 30, y + 9, 1, C_WHITE, row->on ? "ON" : "OFF");
            } else if (row->count > 1) {
                Text(sFont18, listW - 12, y + 7, 2, C_DIM, "x%d", row->count);
            }
            if (row->equipped)
                Text(sFont12, listW - (row->count > 1 ? 60 : 12), y + 12, 2, C_GOLD, sTab == TAB_BOOK ? "ACTIVE" : "EQUIPPED");
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

static void DrawBooks(void) {
    BuildRows();
    Text(sFont14, 14, BODY_Y + 2, 0, C_DIM, "Spell books - tap to select the active book");
    DrawList(BODY_Y + 26, 6 * ROW_H, &sScroll[TAB_BOOK][0]);
    const int dy = BODY_Y + 26 + 6 * ROW_H + 2;
    int sel = sSelected;
    if (sel < 0 || sel > 4)
        for (int i = 0; i < 5; i++)
            if ((sSt.bookSel & 0x7F) == (1 << i))
                sel = i;
    DrawDescBox(dy, TAB_Y - 6 - dy, sel >= 0 && sel < 5 ? kBookName[sel] : "", sel >= 0 && sel < 5 ? sStr[kBookNameIdx[sel] + 213] : "");
}

static void DrawEquip(void) {
    static const char* const names[SUB_COUNT] = { "WHIP", "BODY", "SLOT 1", "SLOT 2", "SLOT 3" };
    DrawSubtabs(names, SUB_COUNT, sSub[TAB_EQUIP]);
    const int c = sSub[TAB_EQUIP];
    const int y = BODY_Y + 38;
    FillRect(4, y, W - 8, 26, C_PANEL);
    const int cur[SUB_COUNT] = { sSt.eqWhip, sSt.eqBody, sSt.eqAcc[0], sSt.eqAcc[1], sSt.eqAcc[2] };
    const int cx[SUB_COUNT] = { 10, 100, 236, 380, 512 };
    (void)cx;
    Text(sFont12, 10, y + 6, 0, c == 0 ? C_GOLD : C_CREAM, "%s", cur[0] >= 0 ? ItemName(cur[0]) : "---");
    Text(sFont12, 130, y + 6, 0, c == 1 ? C_GOLD : C_CREAM, "%s", cur[1] >= 0 ? ItemName(cur[1]) : "---");
    Text(sFont12, 270, y + 6, 0, c == 2 ? C_GOLD : C_CREAM, "%s", cur[2] >= 0 ? ItemName(cur[2]) : "---");
    Text(sFont12, 400, y + 6, 0, c == 3 ? C_GOLD : C_CREAM, "%s", cur[3] >= 0 ? ItemName(cur[3]) : "---");
    Text(sFont12, 520, y + 6, 0, c == 4 ? C_GOLD : C_CREAM, "%s", cur[4] >= 0 ? ItemName(cur[4]) : "---");
    BuildRows();
    DrawList(y + 30, 6 * ROW_H, &sScroll[TAB_EQUIP][c]);
    int sel = sSelected >= 88 && sSelected <= 224 ? sSelected : cur[c];
    DrawDescBox(y + 30 + 6 * ROW_H + 2, TAB_Y - 6 - (y + 30 + 6 * ROW_H + 2), sel >= 0 ? ItemName(sel) : "", sel >= 0 ? ItemDesc(sel) : "");
}

static void DrawItems(void) {
    BuildRows();
    const int listH = 7 * ROW_H;
    DrawList(BODY_Y, listH, &sScroll[TAB_ITEM][0]);
    if (sRowCount == 0)
        Text(sFont18, 280, BODY_Y + 100, 1, C_DIM, "No items.");
    const int dy = BODY_Y + listH + 2;
    DrawDescBox(dy, TAB_Y - 6 - dy, sSelected >= 60 && sSelected <= 87 ? ItemName(sSelected) : "", sSelected >= 60 && sSelected <= 87 ? ItemDesc(sSelected) : "Tap an item.");
}

static void DrawRelics(void) {
    BuildRows();
    Text(sFont14, 14, BODY_Y + 2, 0, C_DIM, "Relics (magic items) - tap to switch ON / OFF");
    DrawList(BODY_Y + 26, 6 * ROW_H, &sScroll[TAB_RELIC][0]);
    const int dy = BODY_Y + 26 + 6 * ROW_H + 2;
    DrawDescBox(dy, TAB_Y - 6 - dy, sSelected >= 230 ? ItemName(sSelected) : "", sSelected >= 230 ? ItemDesc(sSelected) : "");
}

static const char* kElem[8] = { "Fire", "Ice", "Thunder", "Wind", "Holy", "Poison", "Curse", "Dark" };
static void ElemList(char* out, size_t cap, unsigned bits) {
    out[0] = 0;
    for (int i = 0; i < 8; i++)
        if (bits & (1u << i)) {
            if (out[0])
                strncat(out, ", ", cap - strlen(out) - 1);
            strncat(out, kElem[i], cap - strlen(out) - 1);
        }
    if (!out[0])
        snprintf(out, cap, "---");
}

static bool Defeated(int e) { return (sSt.defeated[e / 8] >> (e % 8)) & 1; }

static void DrawEnemies(void) {
    if (sEnemyDetail >= 0) {
        const Enemy* e = &sEnemy[sEnemyDetail];
        FillRect(4, BODY_Y, W - 8, TAB_Y - BODY_Y - 6, C_PANEL);
        FrameRect(4, BODY_Y, W - 8, TAB_Y - BODY_Y - 6, C_GOLD);
        Text(sFont22, 16, BODY_Y + 6, 0, C_GOLD, "No.%02d  %s", sEnemyDetail + 1, sStr[STR_ENEMY_NAME + sEnemyDetail]);
        int y = BODY_Y + 44;
        Text(sFont18, 16, y, 0, C_YELLOW, "LV");
        Text(sFont18, 56, y, 0, C_CREAM, "%u", e->level);
        Text(sFont18, 130, y, 0, C_RED, "HP");
        Text(sFont18, 176, y, 0, C_CREAM, "%u", e->hp);
        Text(sFont18, 280, y, 0, C_GOLD, "EXP");
        Text(sFont18, 334, y, 0, C_CREAM, "%u", e->exp);
        Text(sFont18, 440, y, 0, Defeated(sEnemyDetail) ? C_GREEN : C_DIM, "%s", Defeated(sEnemyDetail) ? "defeated" : "not yet defeated");
        y += 34;
        char el[96];
        ElemList(el, sizeof el, e->weak);
        Text(sFont18, 16, y, 0, C_YELLOW, "WEAK");
        Text(sFont18, 106, y, 0, C_CREAM, "%s", el);
        y += 30;
        ElemList(el, sizeof el, e->tol);
        Text(sFont18, 16, y, 0, C_BLUE, "RESIST");
        Text(sFont18, 106, y, 0, C_CREAM, "%s", el);
        y += 30;
        Text(sFont18, 16, y, 0, C_GREEN, "DROPS");
        for (int d = 0; d < 2; d++) {
            char nm[64];
            DropName(nm, sizeof nm, e->drop[d][0], e->drop[d][1]);
            Text(sFont18, 106 + d * 250, y, 0, e->drop[d][1] ? C_CREAM : C_DIM, "%s", nm);
        }
        FillRect(W - 120, BODY_Y + 6, 108, 32, C_RED);
        FrameRect(W - 120, BODY_Y + 6, 108, 32, C_GOLD);
        Text(sFont18, W - 66, BODY_Y + 10, 1, C_WHITE, "BACK");
        AddHit(W - 120, BODY_Y + 6, 108, 32, H_BACK);
        return;
    }
    const int pages = (ENEMY_COUNT + 9) / 10;
    Text(sFont14, 14, BODY_Y + 4, 0, C_DIM, "Encyclopedia - dim = not yet defeated");
    for (int i = 0; i < 10; i++) {
        const int e = sEnemyPage * 10 + i;
        if (e >= ENEMY_COUNT)
            break;
        const int col = i % 2, row = i / 2;
        const int x = 4 + col * 316, y = BODY_Y + 28 + row * 56;
        const bool own = Defeated(e);
        FillRect(x, y, 312, 52, C_PANEL2);
        FrameRect(x, y, 312, 52, own ? C_GOLD : C_DIM);
        Text(sFont18, x + 10, y + 4, 0, own ? C_GOLD : C_DIM, "%02d", e + 1);
        Text(sFont18, x + 48, y + 4, 0, own ? C_CREAM : C_DIM, "%s", sStr[STR_ENEMY_NAME + e]);
        Text(sFont12, x + 10, y + 30, 0, C_DIM, "Lv %u   HP %u   EXP %u", sEnemy[e].level, sEnemy[e].hp, sEnemy[e].exp);
        AddHit(x, y, 312, 52, H_ENEMY_CELL + e);
    }
    const int by = BODY_Y + 28 + 5 * 56 + 2;
    FillRect(4, by, 110, 30, C_PANEL2);
    FrameRect(4, by, 110, 30, C_DIM);
    Text(sFont18, 59, by + 4, 1, C_CREAM, "< PREV");
    AddHit(4, by, 110, 30, H_PAGE_PREV);
    FillRect(W - 114, by, 110, 30, C_PANEL2);
    FrameRect(W - 114, by, 110, 30, C_DIM);
    Text(sFont18, W - 59, by + 4, 1, C_CREAM, "NEXT >");
    AddHit(W - 114, by, 110, 30, H_PAGE_NEXT);
    Text(sFont18, W / 2, by + 4, 1, C_GOLD, "%d / %d", sEnemyPage + 1, pages);
}

static void DrawStatus(const char* l1, const char* l2) {
    FillRect(0, 0, W, H, C_BG);
    Text(sFont22, W / 2, 170, 1, C_GOLD, "HARMONY OF DISSONANCE");
    Text(sFont18, W / 2, 214, 1, C_CREAM, "%s", l1);
    Text(sFont14, W / 2, 246, 1, C_DIM, "%s", l2);
}

static void DrawFrame(void) {
    SDL_SetRenderDrawColor(sRen, C_BG.r, C_BG.g, C_BG.b, 255);
    SDL_RenderClear(sRen);
    sHitCount = 0;
    if (!sConnected) {
        DrawStatus("Waiting for RetroArch...", "Launch Castlevania: Harmony of Dissonance (mGBA core)");
    } else if (!sSt.valid) {
        DrawStatus("Load a saved game", "Choose a file on the title screen to begin");
    } else {
        DrawHeader();
        switch (sTab) {
            case TAB_BOOK: DrawBooks(); break;
            case TAB_EQUIP: DrawEquip(); break;
            case TAB_ITEM: DrawItems(); break;
            case TAB_RELIC: DrawRelics(); break;
            case TAB_ENEMY: DrawEnemies(); break;
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
        case TAB_BOOK:
        case TAB_RELIC: *y0 = BODY_Y + 26; *h = 6 * ROW_H; return 1;
        default: return 0;
    }
}
static int* CurScroll(void) {
    return &sScroll[sTab][sTab == TAB_EQUIP ? sSub[TAB_EQUIP] : 0];
}

static void OnTap(int x, int y) {
    const int id = HitTest(x, y);
    if (id < 0)
        return;
    if (id >= H_TAB && id < H_TAB + TAB_COUNT) {
        sTab = id - H_TAB;
        sEnemyDetail = -1;
        sSelected = -1;
    } else if (id >= H_SUBTAB && id < H_SUBTAB + SUB_COUNT) {
        sSub[sTab] = id - H_SUBTAB;
        sSelected = -1;
    } else if (id >= H_ROW && id < H_ROW + 40) {
        BuildRows();
        ActivateRow(*CurScroll() + (id - H_ROW));
    } else if (id == H_SCROLL_UP) {
        *CurScroll() -= 3;
    } else if (id == H_SCROLL_DN) {
        *CurScroll() += 3;
    } else if (id == H_PAGE_PREV) {
        if (sEnemyPage > 0) sEnemyPage--;
    } else if (id == H_PAGE_NEXT) {
        if ((sEnemyPage + 1) * 10 < ENEMY_COUNT) sEnemyPage++;
    } else if (id == H_BACK) {
        sEnemyDetail = -1;
    } else if (id >= H_ENEMY_CELL && id < H_ENEMY_CELL + ENEMY_COUNT) {
        sEnemyDetail = id - H_ENEMY_CELL;
    }
    fprintf(stderr, "[hod] tap (%d,%d) -> %d\n", x, y, id);
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
    const char* exeDir = argv[0];
    char dir[512] = ".";
    if (strrchr(exeDir, '/')) {
        snprintf(dir, sizeof dir, "%s", exeDir);
        *strrchr(dir, '/') = 0;
    }
    if (getenv("HOD_DATA_DIR"))
        snprintf(dir, sizeof dir, "%s", getenv("HOD_DATA_DIR"));
    if (!RaClient_Open("127.0.0.1", 55355)) {
        fprintf(stderr, "[hod] cannot open the RetroArch command socket\n");
        return 1;
    }
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0 || TTF_Init() != 0) {
        fprintf(stderr, "[hod] SDL init failed: %s\n", SDL_GetError());
        return 1;
    }
    const int displays = SDL_GetNumVideoDisplays();
    const bool windowed = getenv("HOD_WINDOWED") != NULL || displays < 2;
    Uint32 flags = SDL_WINDOW_SHOWN;
    int px = SDL_WINDOWPOS_UNDEFINED, py = SDL_WINDOWPOS_UNDEFINED;
    if (!windowed) {
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_BORDERLESS;
        px = SDL_WINDOWPOS_UNDEFINED_DISPLAY(1);
        py = SDL_WINDOWPOS_UNDEFINED_DISPLAY(1);
    }
    /* the sway config moves windows titled "...Bottom Screen" to DSI-1 */
    sWin = SDL_CreateWindow("Harmony of Dissonance - Bottom Screen", px, py, W, H, flags);
    if (!sWin) {
        fprintf(stderr, "[hod] window failed: %s\n", SDL_GetError());
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
        fprintf(stderr, "[hod] cannot load font %s: %s\n", fontPath, TTF_GetError());
        return 1;
    }
    sTouchFd = OpenBottomTouch();

    /* QA: HOD_TEST_TAPS="ms,x,y;ms,x,y" injects taps through the real tap handler */
    struct { unsigned ms; int x, y; bool done; } taps[32];
    int tapCount = 0;
    if (getenv("HOD_TEST_TAPS")) {
        char* spec = strdup(getenv("HOD_TEST_TAPS"));
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
        if (sPendingTap) {
            sPendingTap = false;
            OnTap(sTapX, sTapY);
        }
        ApplyDrag();
        for (int i = 0; i < tapCount; i++)
            if (!taps[i].done && SDL_GetTicks() - startMs >= taps[i].ms) {
                taps[i].done = true;
                fprintf(stderr, "[hod] test tap (%d,%d)\n", taps[i].x, taps[i].y);
                OnTap(taps[i].x, taps[i].y);
            }

        const Uint32 now = SDL_GetTicks();
        if (now - lastPoll >= 120) {
            lastPoll = now;
            if (PollState()) {
                if (!sConnected)
                    fprintf(stderr, "[hod] connected to RetroArch\n");
                sConnected = true;
                silent = 0;
                if (!sRomLoaded && now - lastRomTry > 1000) {
                    lastRomTry = now;
                    uint8_t code[4] = { 0 };
                    if (RaClient_Read(HOD_ROM_CODE_ADDR, code, 4) && memcmp(code, "ACHE", 4) == 0 && LoadRom()) {
                        sRomLoaded = true;
                        fprintf(stderr, "[hod] ROM data loaded (%s / %s)\n", sStr[STR_ITEM_NAME], sStr[STR_ENEMY_NAME]);
                    } else if (code[0] && memcmp(code, "ACHE", 4) != 0) {
                        fprintf(stderr, "[hod] not Harmony of Dissonance (USA): %.4s\n", code);
                    }
                }
            } else if (++silent > 8u * (sConnected ? MAX_SILENT_SECONDS : STARTUP_GRACE_SECONDS)) {
                fprintf(stderr, "[hod] RetroArch is gone, exiting\n");
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
