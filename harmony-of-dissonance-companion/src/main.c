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
#include <arpa/inet.h>
#include <sys/socket.h>

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
enum { TAB_BOOK, TAB_EQUIP, TAB_ITEM, TAB_RELIC, TAB_ENEMY, TAB_MAP, TAB_COUNT };
static const char* kTabName[TAB_COUNT] = { "SPELLBOOK", "EQUIP", "ITEM", "RELICS", "ENEMY", "MAP" };

enum {
    H_TAB = 100,       /* + tab */
    H_SUBTAB = 120,    /* + 0..4 */
    H_ROW = 200,       /* + visible row index */
    H_SCROLL_UP = 90,
    H_SCROLL_DN = 91,
    H_PAGE_PREV = 93,
    H_PAGE_NEXT = 94,
    H_BUILD = 96,
    H_VIEW = 97,
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
        TTF_Font* tf = TextWidth(sFont18, kTabName[t]) > tw - 12 ? sFont14 : sFont18;
        Text(tf, t * tw + tw / 2, TAB_Y + (tf == sFont18 ? 17 : 19), 1, act ? C_WHITE : C_CREAM, "%s", kTabName[t]);
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

/* ------------------------------------------------------------------------------------------------------------------ */
/* castle map                                                                                                         */
/* ------------------------------------------------------------------------------------------------------------------ */
/* The game draws its map screen (SELECT) as a 240x160 bitmap in BG0's tiles, from the explored-room flags in RAM
 * (two arrays of 32-bit rows at 0x020000C4 and 0x020001FC; only the first copy of each 8-byte row is used) and its own
 * layout data. The companion learns that layout once from the running game ("BUILD LIVE MAP": it writes test patterns
 * into the flags, opens the game's map screen, copies the picture and restores the flags), keeps it in a local file, and
 * from then on draws the map itself from the live flags, so it follows exploration without touching the game screen.
 * Until the layout is built it just keeps a copy of whatever the game's map screen showed last. */
#define MAP_W 240
#define MAP_H 160
#define MAP_CW 60
#define MAP_CH 40
#define MAP_A_ROWS 33
#define MAP_B_ROWS 27
#define MAP_A_ADDR 0x020000C4u
#define MAP_B_ADDR 0x020001FCu

static uint32_t sMapPix[MAP_W * MAP_H]; /* ABGR8888, what the tab shows */
static SDL_Texture* sMapTex;
static bool sMapHave, sMapLive, sMapDirty;
static char sMapCachePath[600], sLayoutPath[600];
static int sPrevTab = TAB_BOOK;

/* layout learned from the game */
static uint8_t sFull[MAP_W * MAP_H]; /* palette indices with every room explored */
static uint8_t sNoab[MAP_W * MAP_H]; /* ... with nothing explored (rooms already known to the map) */
static uint32_t sPal[16];
static uint8_t sOwnType[MAP_CW * MAP_CH]; /* 0 none, 1 flag in array A, 2 flag in array B */
static uint16_t sOwnIdx[MAP_CW * MAP_CH]; /* flag index = row * 32 + bit */
static bool sLayoutReady;
static int sMapView;      /* 0 both castles, 1 castle A, 2 castle B */
static int sFillIdx = 11; /* palette index of a room explored in both castles while the game shows A+B */
static int16_t sDep[MAP_W * MAP_H][4];    /* cells whose rooms make this pixel visible (-1 = none) */
static uint8_t sExpA[MAP_A_ROWS * 8], sExpB[MAP_B_ROWS * 8];
static bool sExpValid;
static int sViewDrawn = -1;

static uint32_t PalToAbgr(const uint8_t* pal, int i) {
    const unsigned v = (unsigned)(pal[2 * i] | (pal[2 * i + 1] << 8));
    const unsigned r = (v & 31) * 255 / 31, g = ((v >> 5) & 31) * 255 / 31, b = ((v >> 10) & 31) * 255 / 31;
    return 0xFF000000u | (b << 16) | (g << 8) | r;
}

static void MapSaveCache(void) {
    if (!sMapCachePath[0] || !sMapHave)
        return;
    FILE* f = fopen(sMapCachePath, "wb");
    if (!f)
        return;
    fwrite("HODMAP1\0", 1, 8, f);
    fwrite(sMapPix, sizeof sMapPix, 1, f);
    fclose(f);
}

static void MapLoadCache(void) {
    FILE* f = fopen(sMapCachePath, "rb");
    if (!f)
        return;
    char magic[8];
    if (fread(magic, 1, 8, f) == 8 && memcmp(magic, "HODMAP1", 7) == 0 && fread(sMapPix, sizeof sMapPix, 1, f) == 1) {
        sMapHave = true;
        sMapDirty = true;
    }
    fclose(f);
}

/* The game's map screen also draws a title, the explored percentage and L/R arrows into the same bitmap; the tab has no use for them. */
static void MapMaskText(void) {
    static const int box[4][4] = { { 0, 0, 96, 34 }, { 168, 0, 240, 24 }, { 0, 134, 32, 160 }, { 204, 134, 240, 160 } };
    for (int k = 0; k < 4; k++)
        for (int y = box[k][1]; y < box[k][3]; y++)
            for (int x = box[k][0]; x < box[k][2]; x++)
                sFull[y * MAP_W + x] = sNoab[y * MAP_W + x] = 0;
}

static void LayoutBuildDeps(void) {
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++) {
            int16_t* d = sDep[y * MAP_W + x];
            d[0] = d[1] = d[2] = d[3] = -1;
            if (!sFull[y * MAP_W + x])
                continue;
            int cx, lx;
            if (x < 2) { cx = -1; lx = x + 2; } else { cx = (x - 2) / 4; lx = (x - 2) % 4; }
            const int cy = y / 4, ly = y % 4;
            int cells[4] = { cx, cy, 0, 0 };
            (void)cells;
#define CELL(ax, ay) (((ax) >= 0 && (ax) < MAP_CW && (ay) >= 0 && (ay) < MAP_CH && sOwnType[(ay) * MAP_CW + (ax)]) ? (int16_t)((ay) * MAP_CW + (ax)) : (int16_t)-1)
            if (lx >= 1 && ly >= 1) {
                d[0] = CELL(cx, cy);
            } else if (ly == 0 && lx >= 1) {
                d[0] = CELL(cx, cy);
                d[1] = CELL(cx, cy - 1);
            } else if (lx == 0 && ly >= 1) {
                d[0] = CELL(cx, cy);
                d[1] = CELL(cx - 1, cy);
            } else {
                d[0] = CELL(cx, cy);
                d[1] = CELL(cx, cy - 1);
                d[2] = CELL(cx - 1, cy);
                d[3] = CELL(cx - 1, cy - 1);
            }
#undef CELL
        }
}

static bool LayoutSave(void) {
    FILE* f = fopen(sLayoutPath, "wb");
    if (!f)
        return false;
    fwrite("HODLAY1\0", 1, 8, f);
    fwrite(sPal, sizeof sPal, 1, f);
    fwrite(sFull, sizeof sFull, 1, f);
    fwrite(sNoab, sizeof sNoab, 1, f);
    fwrite(sOwnType, sizeof sOwnType, 1, f);
    fwrite(sOwnIdx, sizeof sOwnIdx, 1, f);
    fclose(f);
    return true;
}

static void LayoutLoad(void) {
    FILE* f = fopen(sLayoutPath, "rb");
    if (!f)
        return;
    char magic[8];
    if (fread(magic, 1, 8, f) == 8 && memcmp(magic, "HODLAY1", 7) == 0 && fread(sPal, sizeof sPal, 1, f) == 1 &&
        fread(sFull, sizeof sFull, 1, f) == 1 && fread(sNoab, sizeof sNoab, 1, f) == 1 &&
        fread(sOwnType, sizeof sOwnType, 1, f) == 1 && fread(sOwnIdx, sizeof sOwnIdx, 1, f) == 1) {
        MapMaskText();
        LayoutBuildDeps();
        sLayoutReady = true;
    }
    fclose(f);
}

/* every 8-byte row holds two 32-bit words: castle A's flags, then castle B's */
static int FlagBits(const uint8_t* arr, int idx) {
    const int row = idx / 32, bit = idx % 32;
    int r = 0;
    for (int c = 0; c < 2; c++) {
        const uint8_t* p = arr + row * 8 + c * 4;
        const uint32_t w = (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24));
        r |= (int)((w >> bit) & 1) << c;
    }
    return r; /* bit 0 castle A, bit 1 castle B */
}

static int CellExplored(int cell) {
    const int t = sOwnType[cell], i = sOwnIdx[cell];
    if (t == 1)
        return i / 32 < MAP_A_ROWS ? FlagBits(sExpA, i) : 0;
    if (t == 2)
        return i / 32 < MAP_B_ROWS ? FlagBits(sExpB, i) : 0;
    return 0;
}

/* Draws the map from the explored flags in sExpA / sExpB. View 0 = both castles (the game's A+B screen), 1 = castle A, 2 = castle B. */
static void MapRenderLive(void) {
    static uint8_t exp[MAP_CW * MAP_CH];
    const int mask = sMapView == 0 ? 3 : sMapView; /* which castles count as explored in this view */
    for (int c = 0; c < MAP_CW * MAP_CH; c++)
        exp[c] = sOwnType[c] ? (uint8_t)CellExplored(c) : 0;
    for (int p = 0; p < MAP_W * MAP_H; p++) {
        uint8_t idx = sNoab[p];
        if (sFull[p]) {
            const int16_t* d = sDep[p];
            if ((d[0] >= 0 && (exp[d[0]] & mask)) || (d[1] >= 0 && (exp[d[1]] & mask)) || (d[2] >= 0 && (exp[d[2]] & mask)) ||
                (d[3] >= 0 && (exp[d[3]] & mask))) {
                idx = sFull[p];
                if (idx == sFillIdx && d[0] >= 0) { /* room colour: blue castle A, green castle B, cyan both */
                    const int e = exp[d[0]] & mask;
                    idx = e == 3 ? sFillIdx : e == 1 ? 9 : 10;
                }
            }
        }
        sMapPix[p] = sPal[idx & 15];
    }
    sMapHave = true;
    sMapDirty = true;
}

/* Is the game showing its map screen? That is BG0 in mode 0 whose tile map is a plain y*32+x ramp. */
static bool MapScreenOpen(int* hofs, int* vofs, uint32_t* tmBase, uint32_t* chBase) {
    uint8_t io[0x14];
    if (!RaClient_Read(0x04000000, io, sizeof io))
        return false;
    const unsigned dispcnt = (unsigned)(io[0] | (io[1] << 8)), bg0 = (unsigned)(io[8] | (io[9] << 8));
    if ((dispcnt & 7) != 0 || !(dispcnt & 0x100) || (bg0 & 0xC08C) != 0)
        return false;
    *tmBase = 0x06000000u + ((bg0 >> 8) & 31) * 0x800u;
    *chBase = 0x06000000u + ((bg0 >> 2) & 3) * 0x4000u;
    *hofs = (io[0x10] | (io[0x11] << 8)) & 511;
    *vofs = (io[0x12] | (io[0x13] << 8)) & 511;
    uint8_t row0[8], row1[2];
    if (!RaClient_Read(*tmBase, row0, sizeof row0) || !RaClient_Read(*tmBase + 2u * 32, row1, sizeof row1))
        return false;
    return row0[0] == 0 && row0[2] == 1 && row0[4] == 2 && row0[6] == 3 && row1[0] == 0x20;
}

/* Reads the map screen into palette indices. Fails while the screen fades (palette is not the map's yet). */
static bool MapGrabIdx(int hofs, int vofs, uint32_t tmBase, uint32_t chBase, uint8_t* out, uint8_t* palOut) {
    static uint8_t tm[0x800], tiles[0x5000], pal[512];
    if (!RaClient_Read(tmBase, tm, sizeof tm) || !RaClient_Read(chBase, tiles, sizeof tiles) || !RaClient_Read(0x05000000, pal, sizeof pal))
        return false;
    if (!(pal[2] == 0xFF && pal[3] == 0x7F) || !(pal[18] == 0x00 && pal[19] == 0x70) || !(pal[22] == 0x80 && pal[23] == 0x73))
        return false;
    for (int sy = 0; sy < MAP_H; sy++)
        for (int sx = 0; sx < MAP_W; sx++) {
            const int bx = (sx + hofs) & 255, by = (sy + vofs) & 255;
            const unsigned e = (unsigned)(tm[2 * ((by >> 3) * 32 + (bx >> 3))] | (tm[2 * ((by >> 3) * 32 + (bx >> 3)) + 1] << 8));
            const unsigned t = e & 0x3FF;
            const int px = (e & 0x400) ? 7 - (bx & 7) : (bx & 7), py = (e & 0x800) ? 7 - (by & 7) : (by & 7);
            const size_t off = (size_t)t * 32 + (size_t)py * 4 + (size_t)(px >> 1);
            unsigned c = off < sizeof tiles ? tiles[off] : 0;
            out[sy * MAP_W + sx] = (uint8_t)((px & 1) ? (c >> 4) : (c & 15));
        }
    if (palOut)
        memcpy(palOut, pal, 32);
    return true;
}

/* Fallback while no layout exists: keep a copy of the game's map screen whenever it is open. */
static bool MapCopyFromScreen(int hofs, int vofs, uint32_t tm, uint32_t ch) {
    static uint8_t cur[MAP_W * MAP_H], prev[MAP_W * MAP_H], pal[32];
    static bool havePrev;
    if (!MapGrabIdx(hofs, vofs, tm, ch, cur, pal)) {
        havePrev = false;
        return false;
    }
    int diff = 0;
    if (havePrev)
        for (int i = 0; i < MAP_W * MAP_H && diff < 200; i++)
            diff += cur[i] != prev[i];
    memcpy(prev, cur, sizeof cur);
    const bool stable = havePrev && diff < 200;
    havePrev = true;
    if (!stable)
        return false;
    for (int i = 0; i < MAP_W * MAP_H; i++)
        sMapPix[i] = PalToAbgr(pal, cur[i] & 15);
    sMapHave = true;
    sMapDirty = true;
    return true;
}

/* RetroArch's network gamepad (network_remote, UDP 55400): lets the companion press SELECT for the one-time layout build. */
static int sRemoteFd = -1;
static void RemoteButton(int retropadId, int state) {
    if (sRemoteFd < 0)
        sRemoteFd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sRemoteFd < 0)
        return;
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons(55400);
    inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
    uint32_t pkt[5] = { 0, 1, 0, (uint32_t)retropadId, (uint32_t)(state & 0xFFFF) }; /* port, device (joypad), index, id, state */
    sendto(sRemoteFd, pkt, sizeof pkt, 0, (struct sockaddr*)&a, sizeof a);
}
static void PressSelect(void) {
    RemoteButton(2, 1);
    SDL_Delay(120);
    RemoteButton(2, 0);
}

static void DrawBuildProgress(int done, int total, const char* msg) {
    SDL_PumpEvents();
    SDL_SetRenderDrawColor(sRen, C_BG.r, C_BG.g, C_BG.b, 255);
    SDL_RenderClear(sRen);
    Text(sFont22, W / 2, 150, 1, C_GOLD, "BUILDING THE MAP");
    Text(sFont14, W / 2, 190, 1, C_CREAM, "%s", msg);
    Text(sFont14, W / 2, 214, 1, C_DIM, "The game's map screen will open and close on the top screen. Please wait.");
    FillRect(120, 260, 400, 20, C_PANEL2);
    FillRect(120, 260, total ? 400 * done / total : 0, 20, C_GOLD);
    FrameRect(120, 260, 400, 20, C_DIM);
    SDL_RenderPresent(sRen);
}

static bool MapWaitOpen(bool wantOpen, int ms, int* hofs, int* vofs, uint32_t* tm, uint32_t* ch) {
    for (int t = 0; t < ms; t += 50) {
        const bool o = MapScreenOpen(hofs, vofs, tm, ch);
        if (o == wantOpen)
            return true;
        SDL_Delay(50);
    }
    return false;
}

/* Reads the map screen until two reads agree and enough of it is drawn (the screen builds up: palette first, tiles later). */
static bool MapStableGrab(int hofs, int vofs, uint32_t tm, uint32_t ch, uint8_t* out, uint8_t* palOut, int tries) {
    static uint8_t second[MAP_W * MAP_H];
    for (int g = 0; g < tries; g++) {
        if (MapGrabIdx(hofs, vofs, tm, ch, out, palOut) && MapGrabIdx(hofs, vofs, tm, ch, second, NULL)) {
            int diff = 0, drawn = 0;
            for (int i = 0; i < MAP_W * MAP_H; i++) {
                diff += out[i] != second[i];
                drawn += out[i] != 0;
            }
            if (diff < 20 && drawn > 4000)
                return true;
        }
        SDL_Delay(200);
    }
    return false;
}

/* One test: write the flag arrays, open the game's map, copy it (palette indices), close it again. */
static bool MapTestCapture(const uint8_t* a, const uint8_t* b, uint8_t* out, uint8_t* palOut, bool fixMode) {
    if (!RaClient_Write(MAP_A_ADDR, a, MAP_A_ROWS * 8) || !RaClient_Write(MAP_B_ADDR, b, MAP_B_ROWS * 8)) {
        fprintf(stderr, "[hod] map test: write failed\n");
        return false;
    }
    for (int attempt = 0; attempt < 3; attempt++) {
        int hofs, vofs;
        uint32_t tm, ch;
        PressSelect();
        fprintf(stderr, "[hod] map test: select pressed (attempt %d)\n", attempt);
        if (!MapWaitOpen(true, 4000, &hofs, &vofs, &tm, &ch)) {
            fprintf(stderr, "[hod] map test: map screen did not open\n");
            if (MapScreenOpen(&hofs, &vofs, &tm, &ch)) PressSelect();
            SDL_Delay(1500);
            continue;
        }
        SDL_Delay(350);
        bool ok = MapStableGrab(hofs, vofs, tm, ch, out, palOut, 10);
        fprintf(stderr, "[hod] map test: first grab %s (hofs %d vofs %d tm %x ch %x)\n", ok ? "ok" : "failed", hofs, vofs, (unsigned)tm, (unsigned)ch);
        /* the map opens in the view of the player's castle (A, B or A+B); the layout is learned from A+B (L/R switch) */
        for (int fix = 0; fix < 2 && ok && fixMode; fix++) {
            int cnt[16] = { 0 };
            for (int i = 0; i < MAP_W * MAP_H; i++)
                cnt[out[i] & 15]++;
            if (cnt[11] > 2000)
                break;
            const int id = cnt[9] > 2000 ? 11 : 10; /* castle A -> R, castle B -> L */
            RemoteButton(id, 1);
            SDL_Delay(120);
            RemoteButton(id, 0);
            SDL_Delay(700);
            ok = MapStableGrab(hofs, vofs, tm, ch, out, palOut, 10);
            fprintf(stderr, "[hod] map test: after view switch %s (idx9=%d idx10=%d idx11=%d)\n", ok ? "ok" : "failed", cnt[9], cnt[10], cnt[11]);
        }
        PressSelect();
        MapWaitOpen(false, 4000, &hofs, &vofs, &tm, &ch);
        SDL_Delay(250);
        if (ok)
            return true;
    }
    return false;
}

static void FlagRows(uint8_t* out, int rows, int mode, int k) { /* mode 0 none, 1 all, 2 flags whose index has bit k set */
    for (int r = 0; r < rows; r++) {
        uint32_t w = 0;
        for (int c = 0; c < 32; c++) {
            const int i = r * 32 + c;
            if (mode == 1 || (mode == 2 && ((i >> k) & 1)))
                w |= 1u << c;
        }
        for (int j = 0; j < 4; j++)
            out[r * 8 + j] = out[r * 8 + 4 + j] = (uint8_t)(w >> (8 * j));
    }
}

/* Does the game's map open right now? (Not in a cutscene or a menu.) Leaves the map closed again. */
static bool MapProbe(void) {
    int hofs, vofs;
    uint32_t tm, ch;
    PressSelect();
    if (!MapWaitOpen(true, 2500, &hofs, &vofs, &tm, &ch))
        return false;
    SDL_Delay(300);
    PressSelect();
    MapWaitOpen(false, 3000, &hofs, &vofs, &tm, &ch);
    SDL_Delay(300);
    return true;
}

/* Learns the castle layout from the game. Takes about a minute and needs the game in normal play (not in a menu). */
static bool MapBuildLayout(bool manual) {
    static uint8_t aT[MAP_A_ROWS * 8], bT[MAP_B_ROWS * 8], a0[MAP_A_ROWS * 8], b0[MAP_B_ROWS * 8];
    static uint8_t tType[MAP_W * MAP_H], tBit[11][MAP_W * MAP_H];
    uint8_t pal[32];
    int hofs, vofs;
    uint32_t tm, ch;
    if (MapScreenOpen(&hofs, &vofs, &tm, &ch)) {
        if (manual)
            Toast("Close the game's map first");
        return false;
    }
    if (!RaClient_Read(MAP_A_ADDR, a0, sizeof a0) || !RaClient_Read(MAP_B_ADDR, b0, sizeof b0)) {
        if (manual)
            Toast("Cannot read the game");
        return false;
    }
    if (!MapProbe()) { /* cutscene, menu, fade...: nothing was written, try again later */
        if (manual)
            Toast("The map does not open here - try in normal gameplay");
        return false;
    }
    const int total = 14;
    int done = 0;
    bool ok = true;
    DrawBuildProgress(0, total, "Starting...");
    FlagRows(aT, MAP_A_ROWS, 1, 0);
    FlagRows(bT, MAP_B_ROWS, 1, 0);
    ok = ok && MapTestCapture(aT, bT, sFull, pal, true);
    DrawBuildProgress(++done, total, "Whole castle");
    if (ok) {
        int cnt[16] = { 0 };
        for (int p = 0; p < MAP_W * MAP_H; p++)
            cnt[sFull[p] & 15]++;
        fprintf(stderr, "[hod] map build: full-map fill counts idx9=%d idx10=%d idx11=%d idx2=%d\n", cnt[9], cnt[10], cnt[11], cnt[2]);
        if (cnt[11] < 2000) { /* the map is showing castle A or B alone: the layout is learned from the A+B screen */
            RaClient_Write(MAP_A_ADDR, a0, sizeof a0);
            RaClient_Write(MAP_B_ADDR, b0, sizeof b0);
            Toast("Open the game's map, press L/R until it says A+B, then retry");
            return false;
        }
        for (int i = 0; i < 16; i++)
            sPal[i] = PalToAbgr(pal, i);
        FlagRows(aT, MAP_A_ROWS, 0, 0);
        FlagRows(bT, MAP_B_ROWS, 0, 0);
        ok = MapTestCapture(aT, bT, sNoab, NULL, false);
        DrawBuildProgress(++done, total, "Empty map");
    }
    /* which array owns a pixel: array A all set, array B empty */
    FlagRows(aT, MAP_A_ROWS, 1, 0);
    FlagRows(bT, MAP_B_ROWS, 0, 0);
    ok = ok && MapTestCapture(aT, bT, tType, NULL, false);
    DrawBuildProgress(++done, total, "Map layout");
    /* flag index bit by bit: both arrays carry "index has bit k set" at once, the owner type above tells which one counts */
    for (int k = 0; ok && k < 11; k++) {
        FlagRows(aT, MAP_A_ROWS, 2, k);
        FlagRows(bT, MAP_B_ROWS, 2, k);
        ok = MapTestCapture(aT, bT, tBit[k], NULL, false);
        DrawBuildProgress(++done, total, "Map layout");
    }
    RaClient_Write(MAP_A_ADDR, a0, sizeof a0); /* always put the game's flags back */
    RaClient_Write(MAP_B_ADDR, b0, sizeof b0);
    if (!ok) {
        Toast("Map build failed - will retry in normal gameplay");
        return false;
    }
    /* decode: for every drawn pixel, which flag (array A or B) decides it */
    static int16_t decA[MAP_W * MAP_H], decB[MAP_W * MAP_H];
    for (int p = 0; p < MAP_W * MAP_H; p++) {
        decA[p] = decB[p] = -1;
        if (!sFull[p])
            continue;
        int v = 0;
        for (int k = 0; k < 11; k++) if (tBit[k][p] == sFull[p]) v |= 1 << k;
        if (tType[p] == sFull[p]) {
            if (v < MAP_A_ROWS * 32) decA[p] = (int16_t)v;
        } else if (v < MAP_B_ROWS * 32) {
            decB[p] = (int16_t)v;
        }
    }
    memset(sOwnType, 0, sizeof sOwnType);
    memset(sOwnIdx, 0, sizeof sOwnIdx);
    for (int cy = 0; cy < MAP_CH; cy++)
        for (int cx = 0; cx < MAP_CW; cx++) {
            int bestType = 0, bestIdx = 0, bestN = 0;
            int types[12], idxs[12], cnt[12], nOwn = 0;
            for (int j = 1; j < 4; j++)
                for (int i = 0; i < 4; i++) {
                    const int x = 4 * cx + 2 + i, y = 4 * cy + j;
                    if (x >= MAP_W || y >= MAP_H)
                        continue;
                    const int p = y * MAP_W + x;
                    int t = 0, v = 0;
                    if (decA[p] >= 0) { t = 1; v = decA[p]; } else if (decB[p] >= 0) { t = 2; v = decB[p]; } else continue;
                    int o = 0;
                    while (o < nOwn && !(types[o] == t && idxs[o] == v)) o++;
                    if (o == nOwn && nOwn < 12) { types[o] = t; idxs[o] = v; cnt[o] = 0; nOwn++; }
                    if (o < nOwn) cnt[o]++;
                }
            for (int o = 0; o < nOwn; o++)
                if (cnt[o] > bestN) { bestN = cnt[o]; bestType = types[o]; bestIdx = idxs[o]; }
            sOwnType[cy * MAP_CW + cx] = (uint8_t)bestType;
            sOwnIdx[cy * MAP_CW + cx] = (uint16_t)bestIdx;
        }
    MapMaskText();
    LayoutBuildDeps();
    sLayoutReady = true;
    LayoutSave();
    sExpValid = false; /* force a redraw from the live flags */
    Toast("Live map ready");
    return true;
}

static void MapPoll(Uint32 now) {
    static Uint32 last;
    if (now - last < 250)
        return;
    last = now;
    if (sLayoutReady) {
        uint8_t a[MAP_A_ROWS * 8], b[MAP_B_ROWS * 8];
        if (RaClient_Read(MAP_A_ADDR, a, sizeof a) && RaClient_Read(MAP_B_ADDR, b, sizeof b)) {
            if (!sExpValid || sViewDrawn != sMapView || memcmp(a, sExpA, sizeof a) || memcmp(b, sExpB, sizeof b)) {
                sViewDrawn = sMapView;
                memcpy(sExpA, a, sizeof a);
                memcpy(sExpB, b, sizeof b);
                sExpValid = true;
                MapRenderLive();
                MapSaveCache();
            }
        }
        return;
    }
    int hofs, vofs;
    uint32_t tm, ch;
    const bool open = MapScreenOpen(&hofs, &vofs, &tm, &ch);
    /* no layout yet: learn it once, automatically, as soon as the game has been in normal play (all four backgrounds on,
     * no menu, no map) for a few seconds - that is, right after a new game or a loaded file starts */
    static unsigned stable;
    static Uint32 nextAuto;
    uint8_t dc[2];
    if (!open && RaClient_Read(0x04000000, dc, 2) && ((dc[0] | (dc[1] << 8)) & 0x1F00) == 0x1F00)
        stable++;
    else
        stable = 0;
    if (stable >= 12 && now >= nextAuto) {
        stable = 0;
        if (!MapBuildLayout(false))
            nextAuto = now + 8000;
        return;
    }
    if (open && MapCopyFromScreen(hofs, vofs, tm, ch))
        sMapLive = true, sMapDirty = true;
    if (sMapLive && !open) {
        MapSaveCache();
        sMapLive = false;
    }
}

static void DrawMap(void) {
    FillRect(0, 0, W, TAB_Y - 2, C_BG);
    if (sMapHave) {
        if (sMapDirty) {
            if (!sMapTex) {
                sMapTex = SDL_CreateTexture(sRen, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, MAP_W, MAP_H);
                if (sMapTex)
                    SDL_SetTextureScaleMode(sMapTex, SDL_ScaleModeLinear);
            }
            if (sMapTex)
                SDL_UpdateTexture(sMapTex, NULL, sMapPix, MAP_W * 4);
            sMapDirty = false;
        }
        const int mw = 624, mh = 416, mx = (W - mw) / 2, my = 2;
        if (sMapTex) {
            SDL_Rect d = { mx, my, mw, mh };
            SDL_RenderCopy(sRen, sMapTex, NULL, &d);
        }
    }
    if (sLayoutReady) {
        static const char* kView[3] = { "A+B", "CASTLE A", "CASTLE B" };
        const int vx = 8, vy = 6;
        FillRect(vx, vy, 112, 26, C_PANEL2);
        FrameRect(vx, vy, 112, 26, C_GOLD);
        Text(sFont14, vx + 56, vy + 4, 1, C_WHITE, "%s", kView[sMapView]);
        AddHit(vx, vy, 112, 26, H_VIEW);
        return;
    }
    if (!sMapHave) {
        Text(sFont18, W / 2, BODY_Y + 110, 1, C_GOLD, "CASTLE MAP");
        Text(sFont14, W / 2, BODY_Y + 146, 1, C_CREAM, "Press SELECT in the game to open its map; a copy is kept here.");
        Text(sFont14, W / 2, BODY_Y + 168, 1, C_DIM, "Or build a live map that updates by itself as you explore:");
    } else {
    }
    const int bx = sMapHave ? W - 196 : W / 2 - 98, by = sMapHave ? 6 : BODY_Y + 200;
    FillRect(bx, by, 188, 26, C_RED);
    FrameRect(bx, by, 188, 26, C_GOLD);
    Text(sFont14, bx + 94, by + 4, 1, C_WHITE, "BUILD LIVE MAP");
    AddHit(bx, by, 188, 26, H_BUILD);
    if (!sMapHave)
        Text(sFont12, W / 2, by + 34, 1, C_DIM, "One-time, about a minute; the game's map flashes on the top screen.");
}

/* SELECT on the handheld toggles the map tab (the game opens/closes its own map with the same button). */
static int sPadFd[4], sPadCount;
static void OpenPads(void) {
    const char* forced = getenv("HOD_PAD_DEV");
    if (forced) {
        const int fd = open(forced, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd >= 0)
            sPadFd[sPadCount++] = fd;
        fprintf(stderr, "[hod] pad: %s (%s)\n", forced, fd >= 0 ? "ok" : "cannot open");
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
        fprintf(stderr, "[hod] pad: %s %s (%s)\n", path, name, fd >= 0 ? "ok" : "cannot open");
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
    fprintf(stderr, "[hod] SELECT pressed\n");
    if (sTab == TAB_MAP) {
        sTab = sPrevTab;
    } else {
        sPrevTab = sTab;
        sTab = TAB_MAP;
    }
    sEnemyDetail = -1;
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
        if (sTab != TAB_MAP)
            DrawHeader();
        switch (sTab) {
            case TAB_BOOK: DrawBooks(); break;
            case TAB_EQUIP: DrawEquip(); break;
            case TAB_ITEM: DrawItems(); break;
            case TAB_RELIC: DrawRelics(); break;
            case TAB_ENEMY: DrawEnemies(); break;
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
        if (sTab != TAB_MAP)
            sPrevTab = sTab;
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
    } else if (id == H_BUILD) {
        MapBuildLayout(true);
    } else if (id == H_VIEW) {
        sMapView = (sMapView + 1) % 3;
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
    OpenPads();
    snprintf(sMapCachePath, sizeof sMapCachePath, "%s/map.cache", dir);
    snprintf(sLayoutPath, sizeof sLayoutPath, "%s/map-layout.cache", dir);
    LayoutLoad();
    MapLoadCache();

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
        if (sConnected && sSt.valid)
            MapPoll(now);
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
