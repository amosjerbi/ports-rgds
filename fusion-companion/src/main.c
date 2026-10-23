/* Metroid Fusion (USA) companion for the Anystream RG DS: runs on the bottom panel next to RetroArch's mGBA core on the top panel.
 * Reads and writes the game's RAM over RetroArch's network command interface (UDP 55355).
 * Tabs: STATUS (energy / ammo, tap to refill), ITEMS (upgrades, tap to give or take), MAP (the pause screen's sector maps). */
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

#include "ra_client.h"
#include "mf_data.h"

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
    bool valid;
    uint8_t raw[RAM_EQUIP_SIZE];
    int maxE, e, maxM, m, maxP, p;
    uint8_t beams, explosives, suits, security, maps;
    int hours, minutes, seconds;
} State;
static State sSt;
static bool sConnected, sRomOk;

static int U16(const uint8_t* b, int o) { return b[o] | (b[o + 1] << 8); }

static bool PollState(void) {
    uint8_t b[RAM_EQUIP_SIZE], t[4];
    if (!RaClient_Read(RAM_EQUIP, b, sizeof b))
        return false;
    if (!sRomOk) {
        uint8_t code[4] = { 0 };
        if (RaClient_Read(MF_ROM_CODE_ADDR, code, 4) && memcmp(code, "AMTE", 4) == 0) {
            sRomOk = true;
            fprintf(stderr, "[mf] Metroid Fusion (USA) detected\n");
        } else if (code[0] && memcmp(code, "AMTE", 4) != 0) {
            fprintf(stderr, "[mf] not Metroid Fusion (USA): %.4s\n", code);
        }
    }
    memcpy(sSt.raw, b, sizeof b);
    sSt.e = U16(b, EQ_ENERGY);
    sSt.maxE = U16(b, EQ_MAX_ENERGY);
    sSt.m = U16(b, EQ_MISSILES);
    sSt.maxM = U16(b, EQ_MAX_MISSILES);
    sSt.p = b[EQ_PB];
    sSt.maxP = b[EQ_MAX_PB];
    sSt.beams = b[EQ_BEAMS];
    sSt.explosives = b[EQ_EXPLOSIVES];
    sSt.suits = b[EQ_SUITS];
    sSt.security = b[EQ_SECURITY];
    sSt.maps = b[EQ_MAPS];
    sSt.valid = sRomOk && sSt.maxE >= 99 && sSt.maxE <= 2099 && sSt.e <= sSt.maxE;
    if (RaClient_Read(RAM_TIMER, t, 4)) {
        sSt.hours = t[0];
        sSt.minutes = t[1];
        sSt.seconds = t[2];
    }
    return true;
}

static void WriteBytes(uint32_t addr, const void* p, size_t n) {
    RaClient_Write(addr, p, n);
}
static void WriteU16(uint32_t addr, int v) {
    const uint8_t b[2] = { (uint8_t)(v & 255), (uint8_t)(v >> 8) };
    WriteBytes(addr, b, 2);
}

/* ------------------------------------------------------------------------------------------------------------------ */
/* UI state                                                                                                           */
/* ------------------------------------------------------------------------------------------------------------------ */
enum { TAB_STATUS, TAB_ITEMS, TAB_MAP, TAB_COUNT };
static const char* kTabName[TAB_COUNT] = { "STATUS", "ITEMS", "MAP" };
enum {
    H_TAB = 100,
    H_REFILL_E = 110,
    H_REFILL_M = 111,
    H_REFILL_P = 113,
    H_REFILL_ALL = 114,
    H_BEAM = 120,    /* + index */
    H_MISSILE = 130, /* + index */
    H_SUIT = 140,    /* + index */
    H_AREA = 160, /* + area */
    H_SCAN = 170,
    H_AUTO = 171
};
static int sTab = TAB_STATUS;
static char sToast[96];
static Uint32 sToastUntil;

static void Toast(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(sToast, sizeof sToast, fmt, ap);
    va_end(ap);
    sToastUntil = SDL_GetTicks() + 2200;
}

static const Col C_ORANGE = { 232, 128, 40 }, C_GREY = { 60, 54, 72 }, C_TEAL = { 40, 170, 190 };

/* ------------------------------------------------------------------------------------------------------------------ */
/* map: the pause screen's area maps                                                                                  */
/* ------------------------------------------------------------------------------------------------------------------ */
/* The pause screen (Start) draws the area as one 64x64-tile layer. The companion copies that layer for every area
 * (the app presses Start through RetroArch's network gamepad, and copies whatever sector the game shows while you
 * browse the map yourself), keeps the pictures in a local file, and marks an area when the game's visited-tile flags
 * have changed since its picture was taken. */
typedef struct {
    bool have;
    int x, y, w, h; /* bounding box of the map inside the 512x512 layer */
    uint32_t* pix;  /* w*h, ABGR8888 */
    uint32_t flagHash;
    SDL_Texture* tex;
} AreaMap;
static AreaMap sArea[AREA_COUNT];
static int sMapArea = 0;
static uint8_t sFlags[AREA_COUNT][VISITED_AREA_BYTES];
static bool sFlagsValid;
static char sCachePath[600];
static bool sScanning;
static bool sAutoMap = true; /* refresh the map by itself when the game has revealed new tiles */
static int sScanMax = 8;
static char sCfgPath[600];
static void CfgSave(void) {
    FILE* f = fopen(sCfgPath, "w");
    if (f) {
        fprintf(f, "auto=%d\n", sAutoMap);
        fclose(f);
    }
}
static void CfgLoad(void) {
    FILE* f = fopen(sCfgPath, "r");
    int v;
    if (f) {
        if (fscanf(f, "auto=%d", &v) == 1)
            sAutoMap = v != 0;
        fclose(f);
    }
}

static uint32_t Fnv(const uint8_t* p, size_t n) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++)
        h = (h ^ p[i]) * 16777619u;
    return h;
}

static bool AreaStale(int a) {
    return sArea[a].have && sFlagsValid && Fnv(sFlags[a], VISITED_AREA_BYTES) != sArea[a].flagHash;
}
static bool AnyStale(void) {
    for (int a = 0; a < AREA_COUNT; a++)
        if (AreaStale(a))
            return true;
    return false;
}
static bool AnyMap(void) {
    for (int a = 0; a < AREA_COUNT; a++)
        if (sArea[a].have)
            return true;
    return false;
}

static void MapSaveCache(void) {
    if (!sCachePath[0])
        return;
    FILE* f = fopen(sCachePath, "wb");
    if (!f)
        return;
    fwrite("ZMMAP1\0\0", 1, 8, f);
    for (int a = 0; a < AREA_COUNT; a++) {
        const AreaMap* m = &sArea[a];
        const int32_t hdr[6] = { m->have, m->x, m->y, m->w, m->h, (int32_t)m->flagHash };
        fwrite(hdr, sizeof hdr, 1, f);
        if (m->have)
            fwrite(m->pix, 4, (size_t)m->w * m->h, f);
    }
    fclose(f);
}

static void MapLoadCache(void) {
    FILE* f = fopen(sCachePath, "rb");
    if (!f)
        return;
    char magic[8];
    if (fread(magic, 1, 8, f) != 8 || memcmp(magic, "ZMMAP1", 6) != 0) {
        fclose(f);
        return;
    }
    for (int a = 0; a < AREA_COUNT; a++) {
        int32_t hdr[6];
        if (fread(hdr, sizeof hdr, 1, f) != 1)
            break;
        AreaMap* m = &sArea[a];
        if (!hdr[0])
            continue;
        if (hdr[3] <= 0 || hdr[4] <= 0 || hdr[3] > 512 || hdr[4] > 512)
            break;
        m->pix = malloc((size_t)hdr[3] * hdr[4] * 4);
        if (!m->pix || fread(m->pix, 4, (size_t)hdr[3] * hdr[4], f) != (size_t)hdr[3] * hdr[4]) {
            free(m->pix);
            m->pix = NULL;
            break;
        }
        m->have = true;
        m->x = hdr[1];
        m->y = hdr[2];
        m->w = hdr[3];
        m->h = hdr[4];
        m->flagHash = (uint32_t)hdr[5];
    }
    fclose(f);
}

/* BG3 as the pause screen sets it up for the map. */
static bool MapScreenOpen(void) {
    uint8_t io[0x10];
    if (!RaClient_Read(0x04000000, io, sizeof io))
        return false;
    const unsigned dispcnt = (unsigned)(io[0] | (io[1] << 8)), bg3 = (unsigned)(io[14] | (io[15] << 8));
    /* the pause screens (map, status): all of BG1..3 and sprites on, BG3 as a 64x64 layer at screen base 28, char base 2 */
    return (dispcnt & 7) == 0 && (dispcnt & 0x0800) && (bg3 >> 14) == 3 && ((bg3 >> 8) & 31) == 28 && ((bg3 >> 2) & 3) == 2 && !(bg3 & 0x80);
}
static uint16_t sTm[4096];
static uint32_t TilemapHash(void) {
    uint8_t buf[0x2000];
    if (!RaClient_Read(VRAM_BG3_MAP, buf, sizeof buf))
        return 0;
    memcpy(sTm, buf, sizeof buf);
    return Fnv(buf, sizeof buf);
}

/* The tile entry at (tx, ty) of the 64x64 layer. */
static uint16_t TmAt(int tx, int ty) { return sTm[((ty / 32) * 2 + (tx / 32)) * 1024 + (ty % 32) * 32 + (tx % 32)]; }

/* Which area's visited flags the picture in sTm belongs to: the area whose flagged tiles are all drawn (flag bit c of row r = tile (c, r)). */
static int IdentifyArea(const bool* mapped) {
    int best = -1;
    double bestFrac = 0;
    for (int a = 0; a < AREA_COUNT; a++) {
        int ones = 0, hit = 0;
        for (int r = 0; r < 32; r++) {
            const uint8_t* q = sFlags[a] + r * 4;
            const uint32_t w = (uint32_t)(q[0] | (q[1] << 8) | (q[2] << 16) | ((uint32_t)q[3] << 24));
            for (int c = 0; c < 32; c++)
                if ((w >> c) & 1) {
                    ones++;
                    hit += mapped[r * 64 + c];
                }
        }
        if (ones < 3)
            continue;
        const double frac = (double)hit / ones;
        if (frac > bestFrac) {
            bestFrac = frac;
            best = a;
        }
    }
    return bestFrac >= 0.9 ? best : -1;
}

/* Copies the map the pause screen shows (call with the map open and the layer settled). Returns the area it was filed under, or -1. */
static int MapCapture(void) {
    static uint8_t tiles[BG3_CHARS_BYTES], pal[512];
    static uint32_t full[512 * 512];
    if (!RaClient_Read(VRAM_BG3_CHARS, tiles, sizeof tiles) || !RaClient_Read(0x05000000, pal, sizeof pal))
        return -1;
    if (!TilemapHash())
        return -1;
    /* background tiles = the two most frequent entries */
    static int cnt[65536];
    memset(cnt, 0, sizeof cnt);
    for (int i = 0; i < 4096; i++)
        cnt[sTm[i]]++;
    int bg1 = 0, bg2 = 0;
    for (int e = 0; e < 65536; e++)
        if (cnt[e] > cnt[bg1]) {
            bg2 = bg1;
            bg1 = e;
        } else if (e != bg1 && cnt[e] > cnt[bg2]) {
            bg2 = e;
        }
    static bool mapped[64 * 64];
    int x0 = 64, y0 = 64, x1 = -1, y1 = -1;
    for (int ty = 0; ty < 64; ty++)
        for (int tx = 0; tx < 64; tx++) {
            const uint16_t e = TmAt(tx, ty);
            const bool m = e != bg1 && e != bg2;
            mapped[ty * 64 + tx] = m;
            if (m) {
                if (tx < x0) x0 = tx;
                if (tx > x1) x1 = tx;
                if (ty < y0) y0 = ty;
                if (ty > y1) y1 = ty;
            }
        }
    if (x1 < 0 || x1 - x0 > 62)
        return -1;
    const int area = IdentifyArea(mapped);
    if (area < 0)
        return -1;
    uint32_t colour[256];
    for (int i = 0; i < 256; i++) {
        const unsigned v = (unsigned)(pal[2 * i] | (pal[2 * i + 1] << 8));
        const unsigned r = (v & 31) * 255 / 31, g = ((v >> 5) & 31) * 255 / 31, b = ((v >> 10) & 31) * 255 / 31;
        colour[i] = 0xFF000000u | (b << 16) | (g << 8) | r;
    }
    const int bx0 = x0 > 0 ? x0 - 1 : 0, by0 = y0 > 0 ? y0 - 1 : 0, bx1 = x1 < 63 ? x1 + 1 : 63, by1 = y1 < 63 ? y1 + 1 : 63;
    const int tw = bx1 - bx0 + 1, th = by1 - by0 + 1;
    uint32_t* pix = malloc((size_t)tw * 8 * th * 8 * 4);
    if (!pix)
        return -1;
    for (int ty = by0; ty <= by1; ty++)
        for (int tx = bx0; tx <= bx1; tx++) {
            const uint16_t e = TmAt(tx, ty);
            const bool isBg = e == bg1 || e == bg2;
            const unsigned t = e & 0x3FF, bank = e >> 12;
            for (int py = 0; py < 8; py++)
                for (int px = 0; px < 8; px++) {
                    const int sx = (e & 0x400) ? 7 - px : px, sy = (e & 0x800) ? 7 - py : py;
                    const size_t off = (size_t)t * 32 + (size_t)sy * 4 + (size_t)(sx >> 1);
                    unsigned c = off < sizeof tiles ? tiles[off] : 0;
                    c = (sx & 1) ? (c >> 4) : (c & 15);
                    pix[((ty - by0) * 8 + py) * (tw * 8) + (tx - bx0) * 8 + px] = (isBg || !c) ? 0xFF180C10u : colour[bank * 16 + c];
                }
        }
    AreaMap* m = &sArea[area];
    free(m->pix);
    if (m->tex) {
        SDL_DestroyTexture(m->tex);
        m->tex = NULL;
    }
    m->pix = pix;
    m->have = true;
    m->x = bx0 * 8;
    m->y = by0 * 8;
    m->w = tw * 8;
    m->h = th * 8;
    m->flagHash = Fnv(sFlags[area], VISITED_AREA_BYTES);
    (void)full;
    return area;
}

/* RetroArch's network gamepad (network_remote, UDP 55400): Start / L for UPDATE. */
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
enum { PAD_SELECT = 2, PAD_START = 3, PAD_L = 10 };
static void PressPad(int id) {
    RemoteButton(id, 1);
    SDL_Delay(120);
    RemoteButton(id, 0);
}

static void ReadFlags(void) {
    if (RaClient_Read(RAM_VISITED, sFlags, sizeof sFlags))
        sFlagsValid = true;
}

static void DrawScanProgress(int done, int total, const char* msg) {
    SDL_PumpEvents();
    SDL_SetRenderDrawColor(sRen, C_BG.r, C_BG.g, C_BG.b, 255);
    SDL_RenderClear(sRen);
    Text(sFont22, W / 2, 150, 1, C_GOLD, "COPYING THE MAP");
    Text(sFont14, W / 2, 190, 1, C_CREAM, "%s", msg);
    Text(sFont14, W / 2, 214, 1, C_DIM, "The pause map opens on the top screen for a moment. Please wait.");
    FillRect(120, 260, 400, 20, C_PANEL2);
    FillRect(120, 260, total ? 400 * done / total : 0, 20, C_GOLD);
    FrameRect(120, 260, 400, 20, C_DIM);
    SDL_RenderPresent(sRen);
}

static bool WaitMap(bool wantOpen, int ms) {
    for (int t = 0; t < ms; t += 100) {
        if (MapScreenOpen() == wantOpen)
            return true;
        SDL_Delay(100);
    }
    return false;
}

/* The map screen's layer is mostly one repeated grid tile; the status screen's is not. Needs sTm from TilemapHash(). */
static bool IsMapLayer(void) {
    static int cnt[65536];
    memset(cnt, 0, sizeof cnt);
    int top = 0;
    for (int i = 0; i < 4096; i++)
        if (++cnt[sTm[i]] > top)
            top = cnt[sTm[i]];
    return top >= 2800;
}

/* Waits until the layer has stopped changing, then copies it. */
static int SettleAndCapture(void) {
    uint32_t prev = 0;
    for (int i = 0; i < 20; i++) {
        const uint32_t h = TilemapHash();
        if (h && h == prev)
            return MapCapture();
        prev = h;
        SDL_Delay(250);
    }
    return -1;
}

/* Opens the pause screen, copies the map of the sector Samus is in (the game only shows that sector), closes it again. */
static bool MapScanAll(bool manual) {
    if (MapScreenOpen()) {
        if (manual)
            Toast("Close the game's map first");
        return false;
    }
    ReadFlags();
    if (!sFlagsValid)
        return false;
    PressPad(PAD_START);
    if (!WaitMap(true, 3000)) {
        if (manual)
            Toast("The map does not open here - try in normal gameplay");
        return false;
    }
    sScanning = true;
    DrawScanProgress(1, 3, "Copying the map of this sector");
    int got = -1;
    for (int attempt = 0; attempt < 3 && got < 0; attempt++) {
        /* the pause screen opens on the screen you left it on: the status screen has no map layer, L goes back to the map */
        const uint32_t h = TilemapHash();
        if (h && IsMapLayer())
            got = SettleAndCapture();
        else
            PressPad(PAD_L);
        if (got < 0)
            SDL_Delay(900);
    }
    PressPad(PAD_START);
    WaitMap(false, 3000);
    SDL_Delay(300);
    sScanning = false;
    if (got >= 0) {
        fprintf(stderr, "[mf] scan: %s\n", kAreaName[got]);
        sMapArea = got;
        MapSaveCache();
    }
    if (manual || got >= 0)
        Toast(got >= 0 ? "Copied the map of %s" : "No map found", got >= 0 ? kAreaName[got] : "");
    return got >= 0;
}
static void MapPoll(Uint32 now) {
    static Uint32 last, nextAuto;
    static unsigned stable;
    static uint32_t lastHash;
    if (now - last < 300 || sScanning)
        return;
    last = now;
    ReadFlags();
    uint8_t dc[2];
    const bool open = MapScreenOpen();
    if (open) {
        const uint32_t h = TilemapHash();
        if (h && h != lastHash) {
            /* the user is browsing the game's map: keep a copy of what it shows once the picture has settled */
            SDL_Delay(300);
            if (TilemapHash() == h) {
                const int a = MapCapture();
                if (a >= 0) {
                    lastHash = h;
                    MapSaveCache();
                    sMapArea = a;
                }
            }
        }
        stable = 0;
        return;
    }
    lastHash = 0;
    /* normal play for a few seconds (all four backgrounds on, no menu, no map) */
    if (RaClient_Read(0x04000000, dc, 2) && ((dc[0] | (dc[1] << 8)) & 0x0F00) == 0x0F00 && sSt.valid)
        stable++;
    else
        stable = 0;
    if (stable < 10 || now < nextAuto)
        return;
    if (!AnyMap()) {
        /* no maps yet: copy them once, automatically */
        stable = 0;
        if (!MapScanAll(false))
            nextAuto = now + 10000;
    } else if (sAutoMap && AnyStale()) {
        /* the game revealed new tiles: copy the map again (only the area Samus is in can have changed) */
        stable = 0;
        nextAuto = now + 15000;
        sScanMax = 1;
        MapScanAll(false);
        sScanMax = 8;
    }
}

static void DrawMap(void) {
    FillRect(0, 0, W, TAB_Y - 2, C_BG);
    const int chipW = 74;
    for (int a = 0; a < AREA_COUNT; a++) {
        const int x = 4 + a * (chipW + 2), y = 4;
        const bool sel = a == sMapArea, have = sArea[a].have;
        FillRect(x, y, chipW, 28, sel ? C_RED : C_PANEL2);
        FrameRect(x, y, chipW, 28, AreaStale(a) ? C_YELLOW : (sel ? C_GOLD : C_DIM));
        Text(sFont14, x + chipW / 2, y + 5, 1, have ? (sel ? C_WHITE : C_CREAM) : C_DIM, "%s", kAreaShort[a]);
        AddHit(x, y, chipW, 28, H_AREA + a);
    }
    const int bx = W - 90, by = 4;
    FillRect(bx, by, 86, 28, C_PANEL2);
    FrameRect(bx, by, 86, 28, C_GOLD);
    Text(sFont12, bx + 43, by + 7, 1, C_CREAM, "UPDATE");
    AddHit(bx, by, 86, 28, H_SCAN);
    FillRect(W - 110, TAB_Y - 26, 104, 22, sAutoMap ? C_GREEN : C_PANEL2);
    FrameRect(W - 110, TAB_Y - 26, 104, 22, C_DIM);
    Text(sFont12, W - 58, TAB_Y - 23, 1, C_WHITE, "AUTO UPDATE %s", sAutoMap ? "ON" : "OFF");
    AddHit(W - 110, TAB_Y - 26, 104, 22, H_AUTO);
    const AreaMap* m = &sArea[sMapArea];
    if (!m->have) {
        Text(sFont18, W / 2, 150, 1, C_GOLD, "%s", kAreaName[sMapArea]);
        Text(sFont14, W / 2, 190, 1, C_CREAM, "No copy of this map yet.");
        Text(sFont14, W / 2, 214, 1, C_DIM, "Open the game's map (Start) and look at it, or tap UPDATE here.");
        return;
    }
    AreaMap* mm = &sArea[sMapArea];
    if (!mm->tex) {
        mm->tex = SDL_CreateTexture(sRen, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, mm->w, mm->h);
        if (mm->tex) {
            SDL_UpdateTexture(mm->tex, NULL, mm->pix, mm->w * 4);
            SDL_SetTextureScaleMode(mm->tex, SDL_ScaleModeNearest);
        }
    }
    const int availW = W - 8, availH = TAB_Y - 2 - 40;
    double fit = (double)availW / mm->w;
    if ((double)availH / mm->h < fit)
        fit = (double)availH / mm->h;
    if (fit > 4)
        fit = 4;
    const bool whole = fit >= 2 ? (int)fit == fit : fit == 1;
    const double scale = fit >= 1 ? (fit >= 2 ? (double)(int)fit : fit) : 1; /* whole steps from 2x up, else fill the space */
    (void)whole;
    const int dw = (int)(mm->w * scale), dh = (int)(mm->h * scale), dx = (W - dw) / 2, dy = 36 + (availH - dh) / 2;
    if (mm->tex)
        SDL_SetTextureScaleMode(mm->tex, scale == (int)scale ? SDL_ScaleModeNearest : SDL_ScaleModeLinear);
    FrameRect(dx - 2, dy - 2, dw + 4, dh + 4, C_GOLD);
    if (mm->tex) {
        SDL_Rect d = { dx, dy, dw, dh };
        SDL_RenderCopy(sRen, mm->tex, NULL, &d);
    }
    Text(sFont12, 10, TAB_Y - 20, 0, AreaStale(sMapArea) ? C_YELLOW : C_DIM, "%s%s", kAreaName[sMapArea],
         AreaStale(sMapArea) ? " - explored since this copy; open the game's map or tap UPDATE" : "");
}

/* ------------------------------------------------------------------------------------------------------------------ */
/* drawing                                                                                                            */
/* ------------------------------------------------------------------------------------------------------------------ */
static void DrawHeader(void) {
    FillRect(0, 0, W, HDR_H, C_PANEL);
    FillRect(0, HDR_H - 2, W, 2, C_GOLD);
    Text(sFont22, 14, 14, 0, C_GOLD, "SAMUS ARAN");
    Text(sFont18, W - 14, 6, 2, C_CREAM, "%d:%02d:%02d", sSt.hours, sSt.minutes, sSt.seconds);
    Text(sFont12, W - 14, 34, 2, C_DIM, "play time");
    Text(sFont14, 190, 20, 0, C_DIM, "Metroid Fusion");
}
static void DrawTabs(void) {
    FillRect(0, TAB_Y - 2, W, H - TAB_Y + 2, C_PANEL);
    FillRect(0, TAB_Y - 2, W, 2, C_GOLD);
    const int tw = W / TAB_COUNT;
    for (int t = 0; t < TAB_COUNT; t++) {
        const bool act = sTab == t;
        FillRect(t * tw + 3, TAB_Y + 3, tw - 6, TAB_H - 6, act ? C_RED : C_PANEL2);
        FrameRect(t * tw + 3, TAB_Y + 3, tw - 6, TAB_H - 6, act ? C_GOLD : C_DIM);
        const bool stale = t == TAB_MAP && AnyStale();
        Text(sFont18, t * tw + tw / 2, TAB_Y + 17, 1, act ? C_WHITE : (stale ? C_YELLOW : C_CREAM), "%s", stale ? "MAP !" : kTabName[t]);
        AddHit(t * tw, TAB_Y, tw, TAB_H, H_TAB + t);
    }
}

static void DrawBar(int x, int y, int w, int h, int cur, int max, Col c) {
    FillRect(x, y, w, h, C_PANEL2);
    if (max > 0)
        FillRect(x, y, (int)((long)w * (cur > max ? max : cur) / max), h, c);
    FrameRect(x, y, w, h, C_DIM);
}

static void DrawAmmoRow(int y, const char* name, int cur, int max, Col c, int hit) {
    FillRect(4, y, W - 8, 56, C_PANEL);
    FrameRect(4, y, W - 8, 56, C_DIM);
    Text(sFont18, 18, y + 16, 0, max ? C_CREAM : C_DIM, "%s", name);
    if (max) {
        DrawBar(210, y + 18, 270, 20, cur, max, c);
        Text(sFont18, W - 18, y + 16, 2, cur >= max ? C_GREEN : C_CREAM, "%d / %d", cur, max);
    } else {
        Text(sFont14, 210, y + 19, 0, C_DIM, "not found yet");
    }
    AddHit(4, y, W - 8, 56, hit);
}

static void DrawStatus(void) {
    const int y = BODY_Y + 2;
    /* energy */
    FillRect(4, y, W - 8, 116, C_PANEL);
    FrameRect(4, y, W - 8, 116, C_DIM);
    Text(sFont18, 18, y + 10, 0, C_CREAM, "ENERGY");
    Text(sFont22, W - 18, y + 8, 2, sSt.e >= sSt.maxE ? C_GREEN : C_CREAM, "%d / %d", sSt.e, sSt.maxE);
    const int tanks = sSt.maxE / 100;
    const int tw = tanks > 14 ? 18 : 24, gap = tanks > 14 ? 4 : 6;
    for (int i = 0; i < tanks && i < 20; i++) {
        const int x = 18 + i * (tw + gap);
        const int inTank = sSt.e - i * 100;
        const int fill = inTank >= 100 ? 20 : (inTank <= 0 ? 0 : 20 * inTank / 100);
        FillRect(x, y + 46, tw, 20, C_PANEL2);
        FillRect(x, y + 46 + 20 - fill, tw, fill, C_ORANGE);
        FrameRect(x, y + 46, tw, 20, C_DIM);
    }
    DrawBar(18, y + 80, W - 44, 20, sSt.e, sSt.maxE, C_ORANGE);
    for (int i = 1; i * 100 < sSt.maxE; i++)
        FillRect(18 + (int)((long)(W - 44) * (i * 100) / sSt.maxE), y + 80, 2, 20, C_BG);
    AddHit(4, y, W - 8, 116, H_REFILL_E);
    DrawAmmoRow(y + 122, "MISSILES", sSt.m, sSt.maxM, C_GREEN, H_REFILL_M);
    DrawAmmoRow(y + 182, "POWER BOMBS", sSt.p, sSt.maxP, C_YELLOW, H_REFILL_P);
    Text(sFont14, 18, y + 250, 0, C_DIM, "Security level");
    for (int i = 0; i < 4; i++) { /* the four lock colours of the game: blue, green, yellow, red */
        const Col lock[4] = { C_BLUE, C_GREEN, C_YELLOW, C_RED };
        const bool open = i < sSt.security;
        FillRect(150 + i * 36, y + 246, 30, 24, open ? lock[i] : C_PANEL2);
        FrameRect(150 + i * 36, y + 246, 30, 24, C_DIM);
        Text(sFont12, 165 + i * 36, y + 251, 1, open ? C_WHITE : C_GREY, "%d", i + 1);
    }
    const int by = y + 304;
    FillRect(4, by, W - 8, 44, C_RED);
    FrameRect(4, by, W - 8, 44, C_GOLD);
    Text(sFont18, W / 2, by + 11, 1, C_WHITE, "REFILL ALL");
    AddHit(4, by, W - 8, 44, H_REFILL_ALL);
}
static void DrawItemRow(int x, int y, const char* name, bool has, int hit) {
    FillRect(x, y, 306, 38, has ? C_PANEL2 : C_PANEL);
    FrameRect(x, y, 306, 38, has ? C_GOLD : C_GREY);
    Text(sFont18, x + 12, y + 8, 0, has ? C_CREAM : C_GREY, "%s", name);
    FillRect(x + 222, y + 7, 72, 24, has ? C_GREEN : C_GREY);
    Text(sFont14, x + 258, y + 10, 1, C_WHITE, has ? "HAVE" : "NO");
    AddHit(x, y, 306, 38, hit);
}
static void DrawItems(void) {
    Text(sFont14, 8, BODY_Y + 2, 0, C_GOLD, "BEAMS AND MISSILES");
    for (int i = 0; i < BEAM_COUNT; i++)
        DrawItemRow(4, BODY_Y + 24 + i * 42, kBeamName[i], sSt.beams & kBeamBit[i], H_BEAM + i);
    for (int i = 0; i < MISSILE_COUNT; i++)
        DrawItemRow(4, BODY_Y + 24 + (BEAM_COUNT + i) * 42, kMissileName[i], sSt.explosives & kMissileBit[i], H_MISSILE + i);
    Text(sFont14, W / 2 + 8, BODY_Y + 2, 0, C_GOLD, "SUIT AND ABILITIES");
    for (int i = 0; i < SUIT_COUNT; i++)
        DrawItemRow(W / 2 + 4, BODY_Y + 24 + i * 42, kSuitName[i], sSt.suits & kSuitBit[i], H_SUIT + i);
    /* downloaded maps */
    const int y = BODY_Y + 24 + 7 * 42 + 6;
    Text(sFont12, W / 2 + 8, y, 0, C_DIM, "Maps downloaded");
    for (int a = 0; a < AREA_COUNT; a++) {
        const bool got = sSt.maps & (1 << a);
        const int x = W / 2 + 4 + a * 44;
        FillRect(x, y + 16, 42, 20, got ? C_TEAL : C_PANEL);
        FrameRect(x, y + 16, 42, 20, got ? C_GOLD : C_GREY);
        Text(sFont12, x + 21, y + 19, 1, got ? C_WHITE : C_GREY, "%s", kAreaShort[a]);
    }
}
static void DrawNote(const char* l1, const char* l2) {
    Text(sFont22, W / 2, 180, 1, C_GOLD, "METROID FUSION");
    Text(sFont18, W / 2, 214, 1, C_CREAM, "%s", l1);
    Text(sFont14, W / 2, 246, 1, C_DIM, "%s", l2);
}

static void DrawFrame(void) {
    SDL_SetRenderDrawColor(sRen, C_BG.r, C_BG.g, C_BG.b, 255);
    SDL_RenderClear(sRen);
    sHitCount = 0;
    if (!sConnected) {
        DrawNote("Waiting for RetroArch...", "Launch Metroid Fusion (mGBA core)");
    } else if (!sSt.valid) {
        DrawNote("Load a saved game", "Choose a file on the title screen to begin");
    } else {
        if (sTab != TAB_MAP)
            DrawHeader();
        switch (sTab) {
            case TAB_STATUS: DrawStatus(); break;
            case TAB_ITEMS: DrawItems(); break;
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
/* input                                                                                                              */
/* ------------------------------------------------------------------------------------------------------------------ */
static void Refill(int what) {
    if (what == H_REFILL_E || what == H_REFILL_ALL)
        WriteU16(RAM_EQUIP + EQ_ENERGY, sSt.maxE);
    if (what == H_REFILL_M || what == H_REFILL_ALL)
        WriteU16(RAM_EQUIP + EQ_MISSILES, sSt.maxM);
    if (what == H_REFILL_P || what == H_REFILL_ALL)
        WriteBytes(RAM_EQUIP + EQ_PB, &(uint8_t){ (uint8_t)sSt.maxP }, 1);
    Toast("Refilled");
}
static void OnTap(int x, int y) {
    const int id = HitTest(x, y);
    if (id < 0) {
        fprintf(stderr, "[mf] tap (%d,%d) -> nothing (%d hits)\n", x, y, sHitCount);
        return;
    }
    if (id >= H_TAB && id < H_TAB + TAB_COUNT) {
        sTab = id - H_TAB;
    } else if (id >= H_REFILL_E && id <= H_REFILL_ALL) {
        Refill(id);
    } else if (id >= H_BEAM && id < H_BEAM + BEAM_COUNT) {
        const uint8_t v = sSt.beams ^ kBeamBit[id - H_BEAM];
        WriteBytes(RAM_EQUIP + EQ_BEAMS, &v, 1);
    } else if (id >= H_MISSILE && id < H_MISSILE + MISSILE_COUNT) {
        const uint8_t v = sSt.explosives ^ kMissileBit[id - H_MISSILE];
        WriteBytes(RAM_EQUIP + EQ_EXPLOSIVES, &v, 1);
    } else if (id >= H_SUIT && id < H_SUIT + SUIT_COUNT) {
        const uint8_t v = sSt.suits ^ kSuitBit[id - H_SUIT];
        WriteBytes(RAM_EQUIP + EQ_SUITS, &v, 1);
    } else if (id >= H_AREA && id < H_AREA + AREA_COUNT) {
        sMapArea = id - H_AREA;
    } else if (id == H_AUTO) {
        sAutoMap = !sAutoMap;
        CfgSave();
        Toast(sAutoMap ? "Map updates by itself when you explore" : "Auto update off");
    } else if (id == H_SCAN) {
        MapScanAll(true);
    }
    fprintf(stderr, "[mf] tap (%d,%d) -> %d\n", x, y, id);
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
    if (getenv("MF_DATA_DIR"))
        snprintf(dir, sizeof dir, "%s", getenv("MF_DATA_DIR"));
    if (!RaClient_Open("127.0.0.1", 55355)) {
        fprintf(stderr, "[mf] cannot open the RetroArch command socket\n");
        return 1;
    }
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0 || TTF_Init() != 0) {
        fprintf(stderr, "[mf] SDL init failed: %s\n", SDL_GetError());
        return 1;
    }
    const bool windowed = getenv("MF_WINDOWED") != NULL || SDL_GetNumVideoDisplays() < 2;
    Uint32 flags = SDL_WINDOW_SHOWN;
    int px = SDL_WINDOWPOS_UNDEFINED, py = SDL_WINDOWPOS_UNDEFINED;
    if (!windowed) {
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_BORDERLESS;
        px = SDL_WINDOWPOS_UNDEFINED_DISPLAY(1);
        py = SDL_WINDOWPOS_UNDEFINED_DISPLAY(1);
    }
    /* the sway config moves windows titled "...Bottom Screen" to DSI-1 */
    sWin = SDL_CreateWindow("Metroid Fusion - Bottom Screen", px, py, W, H, flags);
    if (!sWin) {
        fprintf(stderr, "[mf] window failed: %s\n", SDL_GetError());
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
        fprintf(stderr, "[mf] cannot load font %s: %s\n", fontPath, TTF_GetError());
        return 1;
    }
    sTouchFd = OpenBottomTouch();
    snprintf(sCachePath, sizeof sCachePath, "%s/maps.cache", dir);
    snprintf(sCfgPath, sizeof sCfgPath, "%s/auto.cfg", dir);
    CfgLoad();
    MapLoadCache();
    for (int a = AREA_COUNT - 1; a >= 0; a--)
        if (sArea[a].have)
            sMapArea = a;

    /* QA: MF_TEST_TAPS="ms,x,y;ms,x,y" injects taps through the real tap handler */
    struct { unsigned ms; int x, y; bool done; } taps[32];
    int tapCount = 0;
    if (getenv("MF_TEST_TAPS")) {
        char* spec = strdup(getenv("MF_TEST_TAPS"));
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
                fprintf(stderr, "[mf] test tap (%d,%d)\n", taps[i].x, taps[i].y);
                OnTap(taps[i].x, taps[i].y);
            }
        const Uint32 now = SDL_GetTicks();
        if (now - lastPoll >= 120) {
            lastPoll = now;
            if (PollState()) {
                if (!sConnected)
                    fprintf(stderr, "[mf] connected to RetroArch\n");
                sConnected = true;
                silent = 0;
            } else if (++silent > 8u * (sConnected ? MAX_SILENT_SECONDS : STARTUP_GRACE_SECONDS)) {
                fprintf(stderr, "[mf] RetroArch is gone, exiting\n");
                break;
            }
        }
        if (sConnected && sSt.valid)
            MapPoll(now);
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
