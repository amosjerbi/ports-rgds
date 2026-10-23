/* aos-bottom: touch companion for Castlevania: Aria of Sorrow running in RetroArch's mGBA core on the RG DS.
 * Runs on the bottom panel (DSI-1). Reads the game's RAM and ROM over RetroArch's network command interface
 * (READ_CORE_MEMORY, UDP 55355) and writes equipment / souls / abilities back (WRITE_CORE_MEMORY).
 * Tabs: SOUL SET, EQUIP, ITEM, ABILITY, ENEMY. It never touches the emulator's video or input. */
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

#include "aos_data.h"
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
    uint16_t drop[2], hp, exp, weak, tol;
    uint8_t soulType;
    uint16_t soulIdx; /* within type, +1 */
} Enemy;

static char sStr[STR_LAST + 1][112];
static Enemy sEnemy[ENEMY_COUNT];
static bool sRomLoaded;

typedef struct {
    uint8_t win[RAM_WINDOW_SIZE];
    bool valid;
    uint8_t items[ITEM_COUNT];
    uint8_t souls[SOUL_COUNT];
    int level, hp, mp, maxHp, maxMp, att, def, str, con, intl, lck, baseStr, baseCon, baseInt, baseLck;
    uint32_t exp, gold, playFrames;
    int eqWeapon, eqArmor, eqAccessory;  /* item index or -1 */
    int eqSoul[3];                       /* soul index (global) or -1 */
    uint8_t abilityMask;
} State;
static State sSt;

static uint16_t Rd16(const uint8_t* b, uint32_t addr) {
    const uint8_t* p = b + (addr - RAM_WINDOW_BASE);
    return (uint16_t)(p[0] | (p[1] << 8));
}
static uint32_t Rd32(const uint8_t* b, uint32_t addr) {
    return (uint32_t)Rd16(b, addr) | ((uint32_t)Rd16(b, addr + 2) << 16);
}
static uint8_t Rd8(const uint8_t* b, uint32_t addr) {
    return b[addr - RAM_WINDOW_BASE];
}

static int SoulTypeOf(int s) {
    for (int t = 3; t >= 0; t--)
        if (s >= kSoulTypeFirst[t])
            return t;
    return 0;
}

static int NibbleCount(const uint8_t* win, int type, int idx) {
    const uint8_t b = Rd8(win, kSoulRam[type] + (uint32_t)(idx / 2));
    return (idx & 1) ? (b >> 4) : (b & 15);
}

/* ------------------------------------------------------------------------------------------------------------------ */
/* ROM text / tables                                                                                                  */
/* ------------------------------------------------------------------------------------------------------------------ */
static void CleanString(char* dst, size_t cap, const uint8_t* src, size_t n) {
    size_t o = 0;
    for (size_t i = 2; i < n && o + 4 < cap; i++) { /* skip the 01 00 header */
        const uint8_t c = src[i];
        if (c == 0x0A)
            break;
        if (c == 0x06) {
            if (o && dst[o - 1] != ' ')
                dst[o++] = ' ';
        } else if (c >= 0x20 && c < 0x7F) {
            dst[o++] = (char)c;
        } else if (c == 0x0D) {
            dst[o++] = 'L';
        } else if (c == 0x10) {
            dst[o++] = 'v';
        } else if (c < 0x20) {
            dst[o++] = '*';
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
    bool ok = text != NULL;
    ok = ok && RaClient_Read(ROM_STRING_PTRS + 4u * STR_FIRST, &ptrs[STR_FIRST], 4u * (STR_LAST - STR_FIRST + 1));
    ok = ok && RaClient_Read(ROM_TEXT_LO, text, textLen);
    uint8_t* et = malloc(ENEMY_COUNT * ENEMY_REC);
    ok = ok && et && RaClient_Read(ROM_ENEMY_TABLE, et, ENEMY_COUNT * ENEMY_REC);
    if (ok) {
        for (int i = STR_FIRST; i <= STR_LAST; i++) {
            const uint32_t off = ptrs[i] - ROM_TEXT_LO;
            if (ptrs[i] < ROM_TEXT_LO || off >= textLen) {
                sStr[i][0] = 0;
                continue;
            }
            CleanString(sStr[i], sizeof sStr[i], text + off, textLen - off > 120 ? 120 : textLen - off);
        }
        for (int e = 0; e < ENEMY_COUNT; e++) {
            const uint8_t* r = et + e * ENEMY_REC;
            sEnemy[e].drop[0] = (uint16_t)(r[0] | (r[1] << 8));
            sEnemy[e].drop[1] = (uint16_t)(r[2] | (r[3] << 8));
            sEnemy[e].hp = (uint16_t)(r[4] | (r[5] << 8));
            sEnemy[e].exp = (uint16_t)(r[8] | (r[9] << 8));
            sEnemy[e].soulType = r[15];
            sEnemy[e].soulIdx = (uint16_t)(r[16] | (r[17] << 8));
            sEnemy[e].weak = (uint16_t)(r[18] | (r[19] << 8));
            sEnemy[e].tol = (uint16_t)(r[20] | (r[21] << 8));
        }
    }
    free(text);
    free(et);
    return ok;
}

static const char* ItemName(int i) { return (i >= 0 && i < ITEM_COUNT) ? sStr[STR_ITEM_NAME + i] : "---"; }
static const char* ItemDesc(int i) { return (i >= 0 && i < ITEM_COUNT) ? sStr[STR_ITEM_DESC + i] : ""; }
static const char* SoulName(int s) { return (s >= 0 && s < SOUL_COUNT) ? sStr[STR_SOUL_NAME + s] : "---"; }
static const char* SoulDesc(int s) { return (s >= 0 && s < SOUL_COUNT) ? sStr[STR_SOUL_DESC + s] : ""; }

/* ------------------------------------------------------------------------------------------------------------------ */
/* live state                                                                                                         */
/* ------------------------------------------------------------------------------------------------------------------ */
static int Clamp999(int v) { return v > 999 ? 999 : v; } /* the game caps displayed stats */

static bool PollState(void) {
    State n;
    memset(&n, 0, sizeof n);
    if (!RaClient_Read(RAM_WINDOW_BASE, n.win, RAM_WINDOW_SIZE))
        return false;
    const uint8_t* w = n.win;
    n.level = Rd8(w, RAM_LEVEL);
    n.hp = Rd16(w, RAM_HP);
    n.mp = Rd16(w, RAM_MP);
    n.maxHp = Rd16(w, RAM_MAXHP);
    n.maxMp = Rd16(w, RAM_MAXMP);
    n.att = Rd16(w, RAM_ATT);
    n.def = Rd16(w, RAM_DEF);
    n.str = Clamp999(Rd16(w, RAM_STR_TOTAL));
    n.con = Clamp999(Rd16(w, RAM_STR_TOTAL + 2));
    n.intl = Clamp999(Rd16(w, RAM_STR_TOTAL + 4));
    n.lck = Clamp999(Rd16(w, RAM_STR_TOTAL + 6));
    n.baseStr = Rd16(w, RAM_BASE_STATS);
    n.baseCon = Rd16(w, RAM_BASE_STATS + 2);
    n.baseInt = Rd16(w, RAM_BASE_STATS + 4);
    n.baseLck = Rd16(w, RAM_BASE_STATS + 6);
    n.exp = Rd32(w, RAM_EXP);
    n.gold = Rd32(w, RAM_GOLD);
    if (!RaClient_Read(RAM_PLAYTIME, &n.playFrames, 4))
        n.playFrames = 0;
    memcpy(n.items, w + (RAM_ITEMS - RAM_WINDOW_BASE), ITEM_COUNT);
    for (int s = 0; s < SOUL_COUNT; s++) {
        const int t = SoulTypeOf(s);
        n.souls[s] = (uint8_t)NibbleCount(w, t, s - kSoulTypeFirst[t]);
    }
    const int wp = Rd8(w, RAM_EQ_WEAPON), ar = Rd8(w, RAM_EQ_ARMOR), ac = Rd8(w, RAM_EQ_ACCESSORY);
    n.eqWeapon = (wp + ITEM_WEAPON_FIRST <= ITEM_WEAPON_LAST) ? wp + ITEM_WEAPON_FIRST : -1;
    n.eqArmor = (ar + ITEM_ARMOR_FIRST <= ITEM_ARMOR_LAST) ? ar + ITEM_ARMOR_FIRST : -1;
    n.eqAccessory = (ac + ITEM_ARMOR_FIRST >= ITEM_ACCESSORY_FIRST && ac + ITEM_ARMOR_FIRST <= ITEM_ACCESSORY_LAST)
                        ? ac + ITEM_ARMOR_FIRST
                        : -1;
    for (int t = 0; t < 3; t++) {
        const int v = Rd8(w, RAM_EQ_BULLET + (uint32_t)t);
        n.eqSoul[t] = (v >= 1 && v <= kSoulTypeCount[t]) ? kSoulTypeFirst[t] + v - 1 : -1;
    }
    n.abilityMask = Rd8(w, RAM_ABILITY_MASK);
    n.valid = n.level >= 1 && n.level <= 99 && n.maxHp > 0 && n.maxHp <= 9999;
    sSt = n;
    return true;
}

static bool WriteBytes(uint32_t addr, const void* data, size_t n) {
    return RaClient_Write(addr, data, n);
}
static bool WriteU8(uint32_t addr, int v) {
    const uint8_t b = (uint8_t)v;
    return WriteBytes(addr, &b, 1);
}
static bool WriteU16(uint32_t addr, int v) {
    const uint8_t b[2] = { (uint8_t)(v & 255), (uint8_t)((v >> 8) & 255) };
    return WriteBytes(addr, b, 2);
}

/* ------------------------------------------------------------------------------------------------------------------ */
/* drawing helpers                                                                                                    */
/* ------------------------------------------------------------------------------------------------------------------ */
static SDL_Window* sWin;
static SDL_Renderer* sRen;
static TTF_Font *sFont18, *sFont14, *sFont22, *sFont12;

typedef struct { uint8_t r, g, b; } Col;
static const Col C_BG = { 16, 12, 24 }, C_PANEL = { 30, 22, 42 }, C_PANEL2 = { 44, 30, 58 }, C_GOLD = { 214, 176, 88 },
                 C_CREAM = { 240, 230, 208 }, C_DIM = { 128, 118, 140 }, C_RED = { 168, 34, 52 }, C_BLUE = { 70, 110, 200 },
                 C_GREEN = { 74, 170, 98 }, C_YELLOW = { 226, 196, 70 }, C_WHITE = { 255, 255, 255 }, C_SEL = { 92, 36, 60 };

static void SetCol(Col c, int a) { SDL_SetRenderDrawColor(sRen, c.r, c.g, c.b, (Uint8)a); }
static void FillRect(int x, int y, int w, int h, Col c) {
    SDL_Rect r = { x, y, w, h };
    SetCol(c, 255);
    SDL_RenderFillRect(sRen, &r);
}
static void FrameRect(int x, int y, int w, int h, Col c) {
    SDL_Rect r = { x, y, w, h };
    SetCol(c, 255);
    SDL_RenderDrawRect(sRen, &r);
}

typedef struct { char key[240]; SDL_Texture* tex; int w, h; Uint32 last; } TexEntry;
static TexEntry sCache[384];

static SDL_Texture* GetText(TTF_Font* f, const char* s, Col c, int* w, int* h) {
    char key[240];
    snprintf(key, sizeof key, "%p|%02x%02x%02x|%s", (void*)f, c.r, c.g, c.b, s);
    const size_t n = sizeof sCache / sizeof sCache[0];
    TexEntry* slot = NULL;
    for (size_t i = 0; i < n; i++) {
        TexEntry* e = &sCache[i];
        if (e->tex && strcmp(e->key, key) == 0) {
            e->last = SDL_GetTicks();
            *w = e->w;
            *h = e->h;
            return e->tex;
        }
        if (!slot || (slot->tex && (!e->tex || e->last < slot->last)))
            slot = e;
    }
    if (slot->tex)
        SDL_DestroyTexture(slot->tex);
    slot->tex = NULL;
    SDL_Surface* surf = TTF_RenderUTF8_Blended(f, s[0] ? s : " ", (SDL_Color){ c.r, c.g, c.b, 255 });
    if (!surf)
        return NULL;
    slot->tex = SDL_CreateTextureFromSurface(sRen, surf);
    slot->w = surf->w;
    slot->h = surf->h;
    snprintf(slot->key, sizeof slot->key, "%s", key);
    slot->last = SDL_GetTicks();
    SDL_FreeSurface(surf);
    *w = slot->w;
    *h = slot->h;
    return slot->tex;
}

/* align: 0 left, 1 centre, 2 right. Returns drawn width. */
static int Text(TTF_Font* f, int x, int y, int align, Col c, const char* fmt, ...) {
    char buf[200];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    int w = 0, h = 0;
    SDL_Texture* t = GetText(f, buf, c, &w, &h);
    if (!t)
        return 0;
    SDL_Rect d = { align == 0 ? x : align == 1 ? x - w / 2 : x - w, y, w, h };
    SDL_RenderCopy(sRen, t, NULL, &d);
    return w;
}

static int TextWidth(TTF_Font* f, const char* s) {
    int w = 0, h = 0;
    TTF_SizeUTF8(f, s, &w, &h);
    return w;
}

/* Word-wrapped paragraph, at most maxLines lines. */
static void Paragraph(TTF_Font* f, int x, int y, int width, int lineH, int maxLines, Col c, const char* s) {
    char line[260] = "", word[96];
    int lines = 0;
    const char* p = s;
    while (*p && lines < maxLines) {
        int n = 0;
        while (*p == ' ')
            p++;
        while (*p && *p != ' ' && n < 90)
            word[n++] = *p++;
        word[n] = 0;
        if (!n)
            break;
        char test[260];
        snprintf(test, sizeof test, "%s%s%s", line, line[0] ? " " : "", word);
        if (line[0] && TextWidth(f, test) > width) {
            Text(f, x, y + lines * lineH, 0, c, "%s", line);
            lines++;
            snprintf(line, sizeof line, "%s", word);
        } else {
            snprintf(line, sizeof line, "%s", test);
        }
    }
    if (line[0] && lines < maxLines)
        Text(f, x, y + lines * lineH, 0, c, "%s", line);
}

/* ------------------------------------------------------------------------------------------------------------------ */
/* touch                                                                                                              */
/* ------------------------------------------------------------------------------------------------------------------ */
typedef struct { int x, y, w, h, id; } Hit;
static Hit sHits[160];
static int sHitCount;
static void AddHit(int x, int y, int w, int h, int id) {
    if (sHitCount < (int)(sizeof sHits / sizeof sHits[0]))
        sHits[sHitCount++] = (Hit){ x, y, w, h, id };
}
static int HitTest(int x, int y) {
    for (int i = sHitCount - 1; i >= 0; i--)
        if (x >= sHits[i].x && x < sHits[i].x + sHits[i].w && y >= sHits[i].y && y < sHits[i].y + sHits[i].h)
            return sHits[i].id;
    return -1;
}

static int sTouchFd = -1;
static int sTx, sTy, sDownX, sDownY, sLastY;
static bool sDown, sHaveX, sHaveY, sMoved, sPendingTap;
static int sTapX, sTapY;

static int OpenBottomTouch(void) {
    const char* forced = getenv("RGDS_TOUCH_DEV");
    char path[128], best[128] = "";
    if (forced) {
        snprintf(best, sizeof best, "%s", forced);
    } else {
        for (int i = 0; i < 32; i++) {
            char name[128] = "", sysfs[256], link[512];
            snprintf(sysfs, sizeof sysfs, "/sys/class/input/event%d/device/name", i);
            FILE* f = fopen(sysfs, "r");
            if (!f)
                continue;
            if (fgets(name, sizeof name, f) && strstr(name, "Goodix")) {
                snprintf(path, sizeof path, "/dev/input/event%d", i);
                snprintf(sysfs, sizeof sysfs, "/sys/class/input/event%d/device", i);
                ssize_t n = readlink(sysfs, link, sizeof link - 1);
                if (n > 0) {
                    link[n] = 0;
                    if (strstr(link, "fe5e0000")) {
                        snprintf(best, sizeof best, "%s", path);
                        fclose(f);
                        break;
                    }
                }
                snprintf(best, sizeof best, "%s", path);
            }
            fclose(f);
        }
    }
    if (!best[0])
        return -1;
    int fd = open(best, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    fprintf(stderr, "[aos] bottom touch: %s (%s)\n", best, fd >= 0 ? "ok" : "cannot open");
    return fd;
}

static int sDragDy; /* accumulated vertical drag since last consumed */

static void PumpTouch(void) {
    struct input_event ev[32];
    ssize_t n;
    while (sTouchFd >= 0 && (n = read(sTouchFd, ev, sizeof ev)) > 0) {
        for (size_t i = 0; i < (size_t)n / sizeof ev[0]; i++) {
            if (ev[i].type == EV_ABS) {
                if (ev[i].code == ABS_MT_POSITION_X || ev[i].code == ABS_X) {
                    sTx = ev[i].value;
                    sHaveX = true;
                } else if (ev[i].code == ABS_MT_POSITION_Y || ev[i].code == ABS_Y) {
                    sTy = ev[i].value;
                    sHaveY = true;
                } else if (ev[i].code == ABS_MT_TRACKING_ID) {
                    if (ev[i].value >= 0) {
                        sDown = true;
                        sMoved = false;
                        sDownX = sDownY = -1;
                    } else if (sDown) {
                        sDown = false;
                        if (!sMoved) {
                            sPendingTap = true;
                            sTapX = sDownX >= 0 ? sDownX : sTx;
                            sTapY = sDownY >= 0 ? sDownY : sTy;
                        }
                    }
                }
            } else if (ev[i].type == EV_KEY && ev[i].code == BTN_TOUCH) {
                if (ev[i].value == 1 && !sDown) {
                    sDown = true;
                    sMoved = false;
                    sDownX = sDownY = -1;
                } else if (ev[i].value == 0 && sDown) {
                    sDown = false;
                    if (!sMoved) {
                        sPendingTap = true;
                        sTapX = sDownX >= 0 ? sDownX : sTx;
                        sTapY = sDownY >= 0 ? sDownY : sTy;
                    }
                }
            } else if (ev[i].type == EV_SYN && ev[i].code == SYN_REPORT && sDown && sHaveX && sHaveY) {
                if (sDownX < 0) {
                    sDownX = sTx;
                    sDownY = sTy;
                    sLastY = sTy;
                } else {
                    if (abs(sTx - sDownX) > 14 || abs(sTy - sDownY) > 14)
                        sMoved = true;
                    if (sMoved) {
                        sDragDy += sTy - sLastY;
                        sLastY = sTy;
                    }
                }
            }
        }
    }
}

/* ------------------------------------------------------------------------------------------------------------------ */
/* UI state                                                                                                           */
/* ------------------------------------------------------------------------------------------------------------------ */
enum { TAB_SOUL, TAB_EQUIP, TAB_ITEM, TAB_ABILITY, TAB_ENEMY, TAB_MAP, TAB_COUNT };
static bool MapStale(void);
static const char* kTabName[TAB_COUNT] = { "SOUL SET", "EQUIP", "ITEM", "ABILITY", "ENEMY", "MAP" };

/* hit ids */
enum {
    H_TAB = 100,       /* + tab */
    H_SUBTAB = 120,    /* + 0..2 */
    H_ROW = 200,       /* + visible row index */
    H_SCROLL_UP = 90,
    H_SCROLL_DN = 91,
    H_ACTION = 92,
    H_PAGE_PREV = 93,
    H_PAGE_NEXT = 94,
    H_BACK = 95,
    H_BUILD = 96,
    H_ENEMY_CELL = 300 /* + enemy index */
};

static int sTab = TAB_SOUL;
static int sSub[TAB_COUNT]; /* soul: type 0..2; equip: 0 weapon 1 armor 2 accessory */
static int sScroll[TAB_COUNT][3];
static int sSelected = -1;   /* selected entry (item/soul index) used for description */
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

/* A list entry */
typedef struct {
    int id;       /* item index / soul index / -1 for the "none" row */
    int count;
    bool equipped;
    bool on;      /* ability on */
} Row;
static Row sRows[160];
static int sRowCount;

static void BuildRows(void) {
    sRowCount = 0;
    if (!sSt.valid)
        return;
    if (sTab == TAB_SOUL) {
        const int t = sSub[TAB_SOUL];
        sRows[sRowCount++] = (Row){ -1, 0, sSt.eqSoul[t] < 0, false };
        for (int i = 0; i < kSoulTypeCount[t]; i++) {
            const int s = kSoulTypeFirst[t] + i;
            if (sSt.souls[s] > 0)
                sRows[sRowCount++] = (Row){ s, sSt.souls[s], sSt.eqSoul[t] == s, false };
        }
    } else if (sTab == TAB_EQUIP) {
        static const int lo[3] = { ITEM_WEAPON_FIRST, ITEM_ARMOR_FIRST, ITEM_ACCESSORY_FIRST };
        static const int hi[3] = { ITEM_WEAPON_LAST, ITEM_ARMOR_LAST, ITEM_ACCESSORY_LAST };
        const int c = sSub[TAB_EQUIP];
        const int cur = c == 0 ? sSt.eqWeapon : c == 1 ? sSt.eqArmor : sSt.eqAccessory;
        sRows[sRowCount++] = (Row){ -1, 0, cur < 0, false };
        for (int i = lo[c]; i <= hi[c]; i++)
            if (sSt.items[i] > 0)
                sRows[sRowCount++] = (Row){ i, sSt.items[i], cur == i, false };
    } else if (sTab == TAB_ITEM) {
        for (int i = ITEM_CONSUMABLE_FIRST; i <= ITEM_CONSUMABLE_LAST; i++)
            if (sSt.items[i] > 0)
                sRows[sRowCount++] = (Row){ i, sSt.items[i], false, false };
    } else if (sTab == TAB_ABILITY) {
        for (int i = 0; i < 6; i++) {
            const int s = kSoulTypeFirst[3] + i;
            sRows[sRowCount++] = (Row){ sSt.souls[s] > 0 ? s : -2, sSt.souls[s], false, (sSt.abilityMask >> i) & 1 };
        }
    }
}

/* ------------------------------------------------------------------------------------------------------------------ */
/* actions                                                                                                            */
/* ------------------------------------------------------------------------------------------------------------------ */
static bool IsHealItem(int i) { return i >= 0 && i <= 7; }

static void UseItem(int i) {
    if (!IsHealItem(i) || sSt.items[i] == 0)
        return;
    int hp = sSt.hp, mp = sSt.mp;
    switch (i) {
        case 0: hp += 100; break;
        case 1: hp += 400; break;
        case 2: hp = sSt.maxHp; break;
        case 3: hp += 29; break;
        case 4: hp += 290; break;
        case 5: mp += sSt.maxMp / 5; break;
        case 6: mp += sSt.maxMp / 2; break;
        case 7: mp = sSt.maxMp; break;
    }
    if (hp > sSt.maxHp) hp = sSt.maxHp;
    if (mp > sSt.maxMp) mp = sSt.maxMp;
    bool ok = WriteU16(RAM_HP, hp) && WriteU16(RAM_MP, mp) && WriteU8(RAM_ITEMS + (uint32_t)i, sSt.items[i] - 1);
    Toast(ok ? "Used %s" : "Write failed", ItemName(i));
}

static void ActivateRow(int r) {
    if (r < 0 || r >= sRowCount)
        return;
    const Row* row = &sRows[r];
    bool ok = true;
    if (sTab == TAB_SOUL) {
        const int t = sSub[TAB_SOUL];
        ok = WriteU8(RAM_EQ_BULLET + (uint32_t)t, row->id < 0 ? 0 : row->id - kSoulTypeFirst[t] + 1);
        Toast(ok ? (row->id < 0 ? "Soul removed" : "Equipped %s") : "Write failed", SoulName(row->id));
        sSelected = row->id;
    } else if (sTab == TAB_EQUIP) {
        const int c = sSub[TAB_EQUIP];
        if (c == 0)
            ok = WriteU8(RAM_EQ_WEAPON, row->id < 0 ? 0xFF : row->id - ITEM_WEAPON_FIRST);
        else if (c == 1)
            ok = WriteU8(RAM_EQ_ARMOR, row->id < 0 ? 0xFF : row->id - ITEM_ARMOR_FIRST);
        else
            ok = WriteU8(RAM_EQ_ACCESSORY, row->id < 0 ? 0xFF : row->id - ITEM_ARMOR_FIRST);
        Toast(ok ? (row->id < 0 ? "Unequipped" : "Equipped %s") : "Write failed", ItemName(row->id));
        sSelected = row->id;
    } else if (sTab == TAB_ITEM) {
        sSelected = row->id;
    } else if (sTab == TAB_ABILITY) {
        if (row->id >= 0) {
            const int bit = row->id - kSoulTypeFirst[3];
            ok = WriteU8(RAM_ABILITY_MASK, sSt.abilityMask ^ (1 << bit));
            Toast(ok ? "%s %s" : "Write failed", SoulName(row->id), row->on ? "OFF" : "ON");
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
        Text(sFont22, 14, 14, 0, C_GOLD, "SOMA CRUZ");
        return;
    }
    Text(sFont22, 12, 4, 0, C_GOLD, "SOMA");
    Text(sFont18, 84, 8, 0, C_CREAM, "Lv %d", sSt.level);
    Text(sFont14, 150, 4, 0, C_RED, "HP");
    Text(sFont14, 174, 4, 0, C_CREAM, "%d/%d", sSt.hp, sSt.maxHp);
    Text(sFont14, 150, 21, 0, C_BLUE, "MP");
    Text(sFont14, 174, 21, 0, C_CREAM, "%d/%d", sSt.mp, sSt.maxMp);
    /* bars */
    FillRect(256, 8, 104, 8, C_BG);
    FillRect(256, 8, sSt.maxHp ? (int)(104.0 * (sSt.hp > sSt.maxHp ? sSt.maxHp : sSt.hp) / sSt.maxHp) : 0, 8, C_RED);
    FillRect(256, 25, 104, 8, C_BG);
    FillRect(256, 25, sSt.maxMp ? (int)(104.0 * sSt.mp / sSt.maxMp) : 0, 8, C_BLUE);
    Text(sFont14, 372, 4, 0, C_GOLD, "ATT");
    Text(sFont14, 404, 4, 0, C_CREAM, "%d", sSt.att);
    Text(sFont14, 372, 21, 0, C_GOLD, "DEF");
    Text(sFont14, 404, 21, 0, C_CREAM, "%d", sSt.def);
    Text(sFont12, 452, 4, 0, C_DIM, "STR %d  CON %d", sSt.str, sSt.con);
    Text(sFont12, 452, 21, 0, C_DIM, "INT %d  LCK %d", sSt.intl, sSt.lck);
    const uint32_t secs = sSt.playFrames / 60;
    Text(sFont12, 12, 38, 0, C_DIM, "EXP %u", sSt.exp);
    Text(sFont12, 130, 38, 0, C_YELLOW, "%u G", sSt.gold);
    Text(sFont12, 628, 38, 2, C_DIM, "%02u:%02u:%02u", secs / 3600, (secs / 60) % 60, secs % 60);
}

static void DrawTabs(void) {
    FillRect(0, TAB_Y - 2, W, H - TAB_Y + 2, C_PANEL);
    FillRect(0, TAB_Y - 2, W, 2, C_GOLD);
    const int tw = W / TAB_COUNT;
    for (int t = 0; t < TAB_COUNT; t++) {
        const bool act = t == sTab;
        FillRect(t * tw + 3, TAB_Y + 3, tw - 6, TAB_H - 6, act ? C_RED : C_PANEL2);
        FrameRect(t * tw + 3, TAB_Y + 3, tw - 6, TAB_H - 6, act ? C_GOLD : C_DIM);
        Text(sFont18, t * tw + tw / 2, TAB_Y + 17, 1, act ? C_WHITE : (t == TAB_MAP && MapStale() ? C_YELLOW : C_CREAM), "%s", (t == TAB_MAP && MapStale()) ? "MAP !" : kTabName[t]);
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

/* Scrollable list of Rows. y0/h: area. Returns visible rows. */
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
            Text(sFont18, 16, y + 7, 0, C_DIM, "---");
        } else {
            const char* nm = (sTab == TAB_SOUL || sTab == TAB_ABILITY) ? SoulName(row->id) : ItemName(row->id);
            Text(sFont18, 16, y + 7, 0, row->equipped ? C_GOLD : C_CREAM, "%s", nm);
            if (sTab == TAB_ABILITY) {
                FillRect(listW - 56, y + 6, 52, ROW_H - 15, row->on ? C_BLUE : C_RED);
                Text(sFont14, listW - 30, y + 9, 1, C_WHITE, row->on ? "ON" : "OFF");
            } else if (!(sTab == TAB_EQUIP && sSub[TAB_EQUIP] != 0 && row->count == 1) && row->count > 0) {
                Text(sFont18, listW - 12, y + 7, 2, C_DIM, "x%d", row->count);
            }
            if (row->equipped)
                Text(sFont12, listW - 60, y + 12, 2, C_GOLD, "EQUIPPED");
        }
        AddHit(4, y, listW, ROW_H - 3, H_ROW + v);
    }
    /* scroll buttons */
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

static int ScrollFor(int tab) { return tab == TAB_EQUIP ? sSub[TAB_EQUIP] : (tab == TAB_SOUL ? sSub[TAB_SOUL] : 0); }

static void DrawSoulOrEquip(void) {
    if (sTab == TAB_SOUL) {
        static const char* const names[3] = { "BULLET", "GUARDIAN", "ENCHANT" };
        DrawSubtabs(names, 3, sSub[TAB_SOUL]);
        const int t = sSub[TAB_SOUL];
        const Col tc[3] = { C_RED, C_BLUE, C_YELLOW };
        /* equipped strip */
        const int y = BODY_Y + 38;
        FillRect(4, y, W - 8, 26, C_PANEL);
        for (int i = 0; i < 3; i++) {
            FillRect(10 + i * 208, y + 7, 10, 10, tc[i]);
            Text(sFont14, 26 + i * 208, y + 4, 0, i == t ? C_GOLD : C_CREAM, "%s", sSt.eqSoul[i] >= 0 ? SoulName(sSt.eqSoul[i]) : "---");
        }
        BuildRows();
        DrawList(y + 30, 6 * ROW_H, &sScroll[TAB_SOUL][t]);
        const int sel = sSelected >= 0 && SoulTypeOf(sSelected) == t ? sSelected : sSt.eqSoul[t];
        DrawDescBox(y + 30 + 6 * ROW_H + 2, TAB_Y - 6 - (y + 30 + 6 * ROW_H + 2), SoulName(sel), SoulDesc(sel));
    } else {
        static const char* const names[3] = { "WEAPON", "ARMOR", "ACCESSORY" };
        DrawSubtabs(names, 3, sSub[TAB_EQUIP]);
        const int c = sSub[TAB_EQUIP];
        const int y = BODY_Y + 38;
        FillRect(4, y, W - 8, 26, C_PANEL);
        const int cur[3] = { sSt.eqWeapon, sSt.eqArmor, sSt.eqAccessory };
        for (int i = 0; i < 3; i++)
            Text(sFont14, 10 + i * 208, y + 4, 0, i == c ? C_GOLD : C_CREAM, "%s", cur[i] >= 0 ? ItemName(cur[i]) : "---");
        BuildRows();
        DrawList(y + 30, 6 * ROW_H, &sScroll[TAB_EQUIP][c]);
        const int sel = sSelected >= ITEM_WEAPON_FIRST && sSelected <= ITEM_ACCESSORY_LAST ? sSelected : cur[c];
        DrawDescBox(y + 30 + 6 * ROW_H + 2, TAB_Y - 6 - (y + 30 + 6 * ROW_H + 2), sel >= 0 ? ItemName(sel) : "", sel >= 0 ? ItemDesc(sel) : "");
    }
}

static void DrawItems(void) {
    BuildRows();
    const int listH = 7 * ROW_H;
    DrawList(BODY_Y, listH, &sScroll[TAB_ITEM][0]);
    if (sRowCount == 0)
        Text(sFont18, 280, BODY_Y + 100, 1, C_DIM, "No items.");
    const int dy = BODY_Y + listH + 2;
    const bool use = IsHealItem(sSelected) && sSt.items[sSelected] > 0;
    DrawDescBox(dy, TAB_Y - 6 - dy, sSelected >= 0 ? ItemName(sSelected) : "", sSelected >= 0 ? ItemDesc(sSelected) : "Tap an item.");
    if (use) {
        FillRect(W - 140, dy + 6, 126, 34, C_RED);
        FrameRect(W - 140, dy + 6, 126, 34, C_GOLD);
        Text(sFont18, W - 77, dy + 12, 1, C_WHITE, "USE");
        AddHit(W - 140, dy + 6, 126, 34, H_ACTION);
    }
}

static void DrawAbility(void) {
    BuildRows();
    Text(sFont14, 14, BODY_Y + 2, 0, C_DIM, "Ability souls - tap to switch ON / OFF");
    DrawList(BODY_Y + 26, 6 * ROW_H, &sScroll[TAB_ABILITY][0]);
    const int dy = BODY_Y + 26 + 6 * ROW_H + 2;
    DrawDescBox(dy, TAB_Y - 6 - dy, sSelected >= kSoulTypeFirst[3] ? SoulName(sSelected) : "", sSelected >= kSoulTypeFirst[3] ? SoulDesc(sSelected) : "");
}

static const char* kElem[8] = { "Slash", "Fire", "Ice", "Thunder", "Poison", "Holy", "Curse", "Dark" };
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

static bool SoulOwned(const Enemy* e, int* count, int* sidx) {
    *count = 0;
    *sidx = -1;
    if (!e->soulIdx)
        return false;
    const int t = e->soulType > 3 ? 3 : e->soulType;
    if (e->soulIdx > kSoulTypeCount[t])
        return false;
    *sidx = kSoulTypeFirst[t] + e->soulIdx - 1;
    *count = sSt.souls[*sidx];
    return *count > 0;
}

static void DrawEnemies(void) {
    if (sEnemyDetail >= 0) {
        const Enemy* e = &sEnemy[sEnemyDetail];
        FillRect(4, BODY_Y, W - 8, TAB_Y - BODY_Y - 6, C_PANEL);
        FrameRect(4, BODY_Y, W - 8, TAB_Y - BODY_Y - 6, C_GOLD);
        Text(sFont22, 16, BODY_Y + 6, 0, C_GOLD, "No.%02d  %s", sEnemyDetail + 1, sStr[STR_ENEMY_NAME + sEnemyDetail]);
        int cnt, sidx;
        const bool own = SoulOwned(e, &cnt, &sidx);
        int y = BODY_Y + 44;
        Text(sFont18, 16, y, 0, C_RED, "HP");
        Text(sFont18, 70, y, 0, C_CREAM, "%u", e->hp);
        Text(sFont18, 200, y, 0, C_GOLD, "EXP");
        Text(sFont18, 256, y, 0, C_CREAM, "%u", e->exp);
        y += 30;
        char el[96];
        ElemList(el, sizeof el, e->weak & 0xFF);
        Text(sFont18, 16, y, 0, C_YELLOW, "WEAK");
        Text(sFont18, 86, y, 0, C_CREAM, "%s", el);
        y += 28;
        ElemList(el, sizeof el, e->tol & 0xFF);
        Text(sFont18, 16, y, 0, C_BLUE, "RESIST");
        Text(sFont18, 100, y, 0, C_CREAM, "%s", el);
        y += 28;
        Text(sFont18, 16, y, 0, C_GREEN, "DROP");
        for (int d = 0; d < 2; d++) {
            const int it = e->drop[d] ? e->drop[d] - 1 : -1;
            Text(sFont18, 86 + d * 270, y, 0, it >= 0 && it < ITEM_COUNT ? C_CREAM : C_DIM, "%s", it >= 0 && it < ITEM_COUNT ? ItemName(it) : "---");
        }
        y += 28;
        Text(sFont18, 16, y, 0, C_RED, "SOUL");
        if (sidx >= 0)
            Text(sFont18, 86, y, 0, own ? C_GOLD : C_DIM, "%s  %s", SoulName(sidx), own ? "(owned)" : "(not yet)");
        else
            Text(sFont18, 86, y, 0, C_DIM, "---");
        y += 34;
        Paragraph(sFont14, 16, y, W - 40, 18, 3, C_CREAM, sStr[STR_ENEMY_DESC + sEnemyDetail]);
        FillRect(W - 120, BODY_Y + 6, 108, 32, C_RED);
        FrameRect(W - 120, BODY_Y + 6, 108, 32, C_GOLD);
        Text(sFont18, W - 66, BODY_Y + 10, 1, C_WHITE, "BACK");
        AddHit(W - 120, BODY_Y + 6, 108, 32, H_BACK);
        return;
    }
    const int pages = (ENEMY_COUNT + 9) / 10;
    Text(sFont14, 14, BODY_Y + 4, 0, C_DIM, "Enemy data - dim = soul not yet obtained");
    for (int i = 0; i < 10; i++) {
        const int e = sEnemyPage * 10 + i;
        if (e >= ENEMY_COUNT)
            break;
        const int col = i % 2, row = i / 2; /* row-major, like the game */
        const int x = 4 + col * 316, y = BODY_Y + 28 + row * 56;
        int cnt, sidx;
        const bool own = SoulOwned(&sEnemy[e], &cnt, &sidx);
        FillRect(x, y, 312, 52, C_PANEL2);
        FrameRect(x, y, 312, 52, own ? C_GOLD : C_DIM);
        Text(sFont18, x + 10, y + 4, 0, own ? C_GOLD : C_DIM, "%02d", e + 1);
        Text(sFont18, x + 48, y + 4, 0, own ? C_CREAM : C_DIM, "%s", sStr[STR_ENEMY_NAME + e]);
        Text(sFont12, x + 10, y + 30, 0, C_DIM, "HP %u   EXP %u", sEnemy[e].hp, sEnemy[e].exp);
        if (own)
            Text(sFont12, x + 302, y + 30, 2, C_GREEN, "soul x%d", cnt);
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
    Text(sFont22, W / 2, 170, 1, C_GOLD, "ARIA OF SORROW");
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
static int sPrevTab = TAB_SOUL;
static bool MapStale(void) { return false; }

/* layout learned from the game */
static uint8_t sFull[MAP_W * MAP_H]; /* palette indices with every room explored */
static uint8_t sNoab[MAP_W * MAP_H]; /* ... with nothing explored (rooms already known to the map) */
static uint32_t sPal[16];
static uint8_t sOwnType[MAP_CW * MAP_CH]; /* 0 none, 1 flag in array A, 2 flag in array B */
static uint16_t sOwnIdx[MAP_CW * MAP_CH]; /* flag index = row * 32 + bit */
static bool sLayoutReady;
static int16_t sDep[MAP_W * MAP_H][4];    /* cells whose rooms make this pixel visible (-1 = none) */
static uint8_t sExpA[MAP_A_ROWS * 8], sExpB[MAP_B_ROWS * 8];
static bool sExpValid;

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
    fwrite("AOSMAP3\0", 1, 8, f);
    fwrite(sMapPix, sizeof sMapPix, 1, f);
    fclose(f);
}

static void MapLoadCache(void) {
    FILE* f = fopen(sMapCachePath, "rb");
    if (!f)
        return;
    char magic[8];
    if (fread(magic, 1, 8, f) == 8 && memcmp(magic, "AOSMAP3", 7) == 0 && fread(sMapPix, sizeof sMapPix, 1, f) == 1) {
        sMapHave = true;
        sMapDirty = true;
    }
    fclose(f);
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
    fwrite("AOSLAY1\0", 1, 8, f);
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
    if (fread(magic, 1, 8, f) == 8 && memcmp(magic, "AOSLAY1", 7) == 0 && fread(sPal, sizeof sPal, 1, f) == 1 &&
        fread(sFull, sizeof sFull, 1, f) == 1 && fread(sNoab, sizeof sNoab, 1, f) == 1 &&
        fread(sOwnType, sizeof sOwnType, 1, f) == 1 && fread(sOwnIdx, sizeof sOwnIdx, 1, f) == 1) {
        LayoutBuildDeps();
        sLayoutReady = true;
    }
    fclose(f);
}

static bool FlagSet(const uint8_t* arr, int idx) {
    const int row = idx / 32, bit = idx % 32;
    const uint32_t w = (uint32_t)(arr[row * 8] | (arr[row * 8 + 1] << 8) | (arr[row * 8 + 2] << 16) | ((uint32_t)arr[row * 8 + 3] << 24));
    return (w >> bit) & 1;
}

static bool CellExplored(int cell) {
    const int t = sOwnType[cell], i = sOwnIdx[cell];
    if (t == 1)
        return i / 32 < MAP_A_ROWS && FlagSet(sExpA, i);
    if (t == 2)
        return i / 32 < MAP_B_ROWS && FlagSet(sExpB, i);
    return false;
}

/* Draws the map from the explored flags in sExpA / sExpB. */
static void MapRenderLive(void) {
    static bool exp[MAP_CW * MAP_CH];
    for (int c = 0; c < MAP_CW * MAP_CH; c++)
        exp[c] = sOwnType[c] && CellExplored(c);
    for (int p = 0; p < MAP_W * MAP_H; p++) {
        uint8_t idx = sNoab[p];
        if (sFull[p]) {
            const int16_t* d = sDep[p];
            if ((d[0] >= 0 && exp[d[0]]) || (d[1] >= 0 && exp[d[1]]) || (d[2] >= 0 && exp[d[2]]) || (d[3] >= 0 && exp[d[3]]))
                idx = sFull[p];
            else if (!idx) /* rooms not explored yet: dim grey outline of the whole castle */
                idx = sFull[p] == 1 ? 5 : 4;
        }
        sMapPix[p] = sPal[idx & 15];
    }
    sMapHave = true;
    sMapDirty = true;
}

/* Is the game showing its map screen? That is BG0 in mode 0 whose tile map is a plain y*32+x ramp starting at (2,2). */
static bool MapScreenOpen(int* hofs, int* vofs, uint32_t* tmBase, uint32_t* chBase) {
    uint8_t io[0x14];
    if (!RaClient_Read(0x04000000, io, sizeof io))
        return false;
    const unsigned dispcnt = (unsigned)(io[0] | (io[1] << 8)), bg0 = (unsigned)(io[8] | (io[9] << 8));
    if ((dispcnt & 7) != 0 || !(dispcnt & 0x100) || (bg0 & 0xC080) != 0)
        return false;
    *tmBase = 0x06000000u + ((bg0 >> 8) & 31) * 0x800u;
    *chBase = 0x06000000u + ((bg0 >> 2) & 3) * 0x4000u;
    *hofs = (io[0x10] | (io[0x11] << 8)) & 511;
    *vofs = (io[0x12] | (io[0x13] << 8)) & 511;
    uint8_t row2[8], row3[2];
    if (!RaClient_Read(*tmBase + 2u * (2 * 32 + 2), row2, sizeof row2) || !RaClient_Read(*tmBase + 2u * (3 * 32 + 2), row3, sizeof row3))
        return false;
    return row2[0] == 0x42 && row2[2] == 0x43 && row2[4] == 0x44 && row2[6] == 0x45 && row3[0] == 0x62;
}

/* Reads the map screen into palette indices. Fails while the screen fades (palette is not the map's yet). */
static bool MapGrabIdx(int hofs, int vofs, uint32_t tmBase, uint32_t chBase, uint8_t* out, uint8_t* palOut) {
    static uint8_t tm[0x800], tiles[0x5000], pal[512];
    if (!RaClient_Read(tmBase, tm, sizeof tm) || !RaClient_Read(chBase, tiles, sizeof tiles) || !RaClient_Read(0x05000000, pal, sizeof pal))
        return false;
    if (!(pal[2] == 0xFF && pal[3] == 0x7F) || !(pal[18] == 0x00 && pal[19] == 0x70) || !(pal[22] == 0x20 && pal[23] == 0x67))
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
static bool MapTestCapture(const uint8_t* a, const uint8_t* b, uint8_t* out, uint8_t* palOut) {
    if (!RaClient_Write(MAP_A_ADDR, a, MAP_A_ROWS * 8) || !RaClient_Write(MAP_B_ADDR, b, MAP_B_ROWS * 8)) {
        fprintf(stderr, "[aos] map test: write failed\n");
        return false;
    }
    for (int attempt = 0; attempt < 3; attempt++) {
        int hofs, vofs;
        uint32_t tm, ch;
        PressSelect();
        fprintf(stderr, "[aos] map test: select pressed (attempt %d)\n", attempt);
        if (!MapWaitOpen(true, 4000, &hofs, &vofs, &tm, &ch)) {
            fprintf(stderr, "[aos] map test: map screen did not open\n");
            if (MapScreenOpen(&hofs, &vofs, &tm, &ch)) PressSelect();
            SDL_Delay(1500);
            continue;
        }
        SDL_Delay(350);
        const bool ok = MapStableGrab(hofs, vofs, tm, ch, out, palOut, 10);
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
    ok = ok && MapTestCapture(aT, bT, sFull, pal);
    DrawBuildProgress(++done, total, "Whole castle");
    if (ok) {
        for (int i = 0; i < 16; i++)
            sPal[i] = PalToAbgr(pal, i);
        FlagRows(aT, MAP_A_ROWS, 0, 0);
        FlagRows(bT, MAP_B_ROWS, 0, 0);
        ok = MapTestCapture(aT, bT, sNoab, NULL);
        DrawBuildProgress(++done, total, "Empty map");
    }
    /* which array owns a pixel: array A all set, array B empty */
    FlagRows(aT, MAP_A_ROWS, 1, 0);
    FlagRows(bT, MAP_B_ROWS, 0, 0);
    ok = ok && MapTestCapture(aT, bT, tType, NULL);
    DrawBuildProgress(++done, total, "Map layout");
    /* flag index bit by bit: both arrays carry "index has bit k set" at once, the owner type above tells which one counts */
    for (int k = 0; ok && k < 11; k++) {
        FlagRows(aT, MAP_A_ROWS, 2, k);
        FlagRows(bT, MAP_B_ROWS, 2, k);
        ok = MapTestCapture(aT, bT, tBit[k], NULL);
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
            if (!sExpValid || memcmp(a, sExpA, sizeof a) || memcmp(b, sExpB, sizeof b)) {
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
    if (sLayoutReady)
        return;
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
    const char* forced = getenv("AOS_PAD_DEV");
    if (forced) {
        const int fd = open(forced, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd >= 0)
            sPadFd[sPadCount++] = fd;
        fprintf(stderr, "[aos] pad: %s (%s)\n", forced, fd >= 0 ? "ok" : "cannot open");
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
        fprintf(stderr, "[aos] pad: %s %s (%s)\n", path, name, fd >= 0 ? "ok" : "cannot open");
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
    fprintf(stderr, "[aos] SELECT pressed\n");
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
        DrawStatus("Waiting for RetroArch...", "Launch Castlevania: Aria of Sorrow (mGBA core)");
    } else if (!sSt.valid) {
        DrawStatus("Load a saved game", "Choose a file on the title screen to begin");
    } else {
        if (sTab != TAB_MAP)
            DrawHeader();
        switch (sTab) {
            case TAB_SOUL:
            case TAB_EQUIP: DrawSoulOrEquip(); break;
            case TAB_ITEM: DrawItems(); break;
            case TAB_ABILITY: DrawAbility(); break;
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
        case TAB_SOUL:
        case TAB_EQUIP: *y0 = BODY_Y + 38 + 30; *h = 6 * ROW_H; return 1;
        case TAB_ITEM: *y0 = BODY_Y; *h = 7 * ROW_H; return 1;
        case TAB_ABILITY: *y0 = BODY_Y + 26; *h = 6 * ROW_H; return 1;
        default: return 0;
    }
}
static int* CurScroll(void) {
    return &sScroll[sTab][(sTab == TAB_SOUL || sTab == TAB_EQUIP) ? sSub[sTab] : 0];
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
    } else if (id >= H_SUBTAB && id < H_SUBTAB + 3) {
        sSub[sTab] = id - H_SUBTAB;
        sSelected = -1;
    } else if (id >= H_ROW && id < H_ROW + 40) {
        BuildRows();
        const int r = *CurScroll() + (id - H_ROW);
        ActivateRow(r);
    } else if (id == H_SCROLL_UP) {
        *CurScroll() -= 3;
    } else if (id == H_SCROLL_DN) {
        *CurScroll() += 3;
    } else if (id == H_ACTION) {
        if (sTab == TAB_ITEM)
            UseItem(sSelected);
    } else if (id == H_PAGE_PREV) {
        if (sEnemyPage > 0) sEnemyPage--;
    } else if (id == H_PAGE_NEXT) {
        if ((sEnemyPage + 1) * 10 < ENEMY_COUNT) sEnemyPage++;
    } else if (id == H_BUILD) {
        MapBuildLayout(true);
    } else if (id == H_BACK) {
        sEnemyDetail = -1;
    } else if (id >= H_ENEMY_CELL && id < H_ENEMY_CELL + ENEMY_COUNT) {
        sEnemyDetail = id - H_ENEMY_CELL;
    }
    fprintf(stderr, "[aos] tap (%d,%d) -> %d\n", x, y, id);
    if (sConnected)
        PollState(); /* show the effect immediately */
}

static void ApplyDrag(void) {
    int y0, h;
    if (abs(sDragDy) < ROW_H / 2 || !ListArea(&y0, &h))
        return;
    if (sTy >= y0 && sTy < y0 + h) {
        const int rows = sDragDy / ROW_H ? sDragDy / ROW_H : (sDragDy > 0 ? 1 : -1);
        *CurScroll() -= rows; /* dragging down shows earlier rows */
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
    if (getenv("AOS_DATA_DIR"))
        snprintf(dir, sizeof dir, "%s", getenv("AOS_DATA_DIR"));
    if (!RaClient_Open("127.0.0.1", 55355)) {
        fprintf(stderr, "[aos] cannot open the RetroArch command socket\n");
        return 1;
    }
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0 || TTF_Init() != 0) {
        fprintf(stderr, "[aos] SDL init failed: %s\n", SDL_GetError());
        return 1;
    }
    const int displays = SDL_GetNumVideoDisplays();
    const bool windowed = getenv("AOS_WINDOWED") != NULL || displays < 2;
    Uint32 flags = SDL_WINDOW_SHOWN;
    int px = SDL_WINDOWPOS_UNDEFINED, py = SDL_WINDOWPOS_UNDEFINED;
    if (!windowed) {
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_BORDERLESS;
        px = SDL_WINDOWPOS_UNDEFINED_DISPLAY(1);
        py = SDL_WINDOWPOS_UNDEFINED_DISPLAY(1);
    }
    /* the sway config moves windows titled "...Bottom Screen" to DSI-1 */
    sWin = SDL_CreateWindow("Aria of Sorrow - Bottom Screen", px, py, W, H, flags);
    if (!sWin) {
        fprintf(stderr, "[aos] window failed: %s\n", SDL_GetError());
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
        fprintf(stderr, "[aos] cannot load font %s: %s\n", fontPath, TTF_GetError());
        return 1;
    }
    sTouchFd = OpenBottomTouch();
    OpenPads();
    snprintf(sMapCachePath, sizeof sMapCachePath, "%s/map.cache", dir);
    snprintf(sLayoutPath, sizeof sLayoutPath, "%s/map-layout.cache", dir);
    LayoutLoad();
    MapLoadCache();

    /* QA: AOS_TEST_TAPS="ms,x,y;ms,x,y" injects taps through the real tap handler */
    struct { unsigned ms; int x, y; bool done; } taps[32];
    int tapCount = 0;
    if (getenv("AOS_TEST_TAPS")) {
        char* spec = strdup(getenv("AOS_TEST_TAPS"));
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
                fprintf(stderr, "[aos] test tap (%d,%d)\n", taps[i].x, taps[i].y);
                OnTap(taps[i].x, taps[i].y);
            }

        const Uint32 now = SDL_GetTicks();
        if (now - lastPoll >= 120) {
            lastPoll = now;
            if (PollState()) {
                if (!sConnected)
                    fprintf(stderr, "[aos] connected to RetroArch\n");
                sConnected = true;
                silent = 0;
                if (!sRomLoaded && now - lastRomTry > 1000) {
                    lastRomTry = now;
                    uint8_t code[4] = { 0 };
                    if (RaClient_Read(AOS_ROM_CODE_ADDR, code, 4) && memcmp(code, "A2CE", 4) == 0 && LoadRom()) {
                        sRomLoaded = true;
                        fprintf(stderr, "[aos] ROM data loaded (%s / %s)\n", sStr[STR_ITEM_NAME], sStr[STR_SOUL_NAME]);
                    } else if (code[0] && memcmp(code, "A2CE", 4) != 0) {
                        fprintf(stderr, "[aos] not Aria of Sorrow (USA): %.4s\n", code);
                    }
                }
            } else if (++silent > 8u * (sConnected ? MAX_SILENT_SECONDS : STARTUP_GRACE_SECONDS)) {
                fprintf(stderr, "[aos] RetroArch is gone, exiting\n");
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
