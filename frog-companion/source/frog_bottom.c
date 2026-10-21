/* frog-bottom: bottom-panel companion for "For the Frog the Bell Tolls" running in RetroArch (gambatte).
 *
 * The game draws its own pause menu (STATUS / HOLD / SELECT / KEY) from a save record in work RAM. This program
 * hosts a second copy of the game inside itself, parked in that menu. When the live game's record changes, the
 * record is copied into the hidden copy, which redraws the menu with its own code; the result is shown on the
 * bottom panel together with a live LIFE / COIN header. Nothing about the menu is re-implemented here, so values
 * the game computes (ARMOR, JUMP, HOLD slots, keys) are exactly what the real pause menu would show.
 *
 * Reads the live game over RetroArch's network command interface (READ_CORE_MEMORY, UDP 55355). */
#include <SDL2/SDL.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libretro_host.h"
#include "ra_client.h"

#define REC_BASE 0xCBC2u /* the active save record in work RAM */
#define REC_LEN 0x7Eu    /* up to 0xCC40: covers status, items, counts, keys */
#define OFF_HP 10        /* current life, quarter hearts */
#define OFF_MAXHP 11     /* maximum life, quarter hearts */
#define OFF_COIN 0x19    /* 24-bit big-endian */

#define W 640
#define H 480
#define GBW 160
#define GBH 144
#define MENU_TOP 16 /* the first 16 lines of the menu frame are the stale LIFE/COIN row: replaced by ours */

static uint32_t sScreen[W * H];
static uint32_t sMenu[GBW * GBH];
static bool sHaveMenu;

/* ------------------------------------------------------------------ painter */
static void Fill(int x, int y, int w, int h, uint32_t c) {
    for (int j = y; j < y + h; j++) {
        if (j < 0 || j >= H)
            continue;
        for (int i = x; i < x + w; i++)
            if (i >= 0 && i < W)
                sScreen[j * W + i] = c;
    }
}

#define RGB(r, g, b) (0xFF000000u | ((uint32_t)(b) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(r))

static const struct { char c; const char* r[7]; } kFont[] = {
    { 'A', { ".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#" } },
    { 'B', { "####.", "#...#", "#...#", "####.", "#...#", "#...#", "####." } },
    { 'C', { ".###.", "#...#", "#....", "#....", "#....", "#...#", ".###." } },
    { 'D', { "####.", "#...#", "#...#", "#...#", "#...#", "#...#", "####." } },
    { 'E', { "#####", "#....", "#....", "####.", "#....", "#....", "#####" } },
    { 'F', { "#####", "#....", "#....", "####.", "#....", "#....", "#...." } },
    { 'G', { ".###.", "#...#", "#....", "#.###", "#...#", "#...#", ".###." } },
    { 'H', { "#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#" } },
    { 'I', { ".###.", "..#..", "..#..", "..#..", "..#..", "..#..", ".###." } },
    { 'L', { "#....", "#....", "#....", "#....", "#....", "#....", "#####" } },
    { 'M', { "#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#", "#...#" } },
    { 'N', { "#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#", "#...#" } },
    { 'O', { ".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###." } },
    { 'P', { "####.", "#...#", "#...#", "####.", "#....", "#....", "#...." } },
    { 'R', { "####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#" } },
    { 'S', { ".####", "#....", "#....", ".###.", "....#", "....#", "####." } },
    { 'T', { "#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.." } },
    { 'U', { "#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###." } },
    { 'V', { "#...#", "#...#", "#...#", "#...#", "#...#", ".#.#.", "..#.." } },
    { 'W', { "#...#", "#...#", "#...#", "#.#.#", "#.#.#", "##.##", "#...#" } },
    { '0', { ".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###." } },
    { '1', { "..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###." } },
    { '2', { ".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####" } },
    { '3', { "#####", "...#.", "..#..", "...#.", "....#", "#...#", ".###." } },
    { '4', { "...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#." } },
    { '5', { "#####", "#....", "####.", "....#", "....#", "#...#", ".###." } },
    { '6', { "..##.", ".#...", "#....", "####.", "#...#", "#...#", ".###." } },
    { '7', { "#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#..." } },
    { '8', { ".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###." } },
    { '9', { ".###.", "#...#", "#...#", ".####", "....#", "...#.", ".##.." } },
    { '/', { "....#", "....#", "...#.", "..#..", ".#...", "#....", "#...." } },
    { '.', { ".....", ".....", ".....", ".....", ".....", ".##..", ".##.." } },
};

static void Glyph(int x, int y, int s, char ch, uint32_t c) {
    for (size_t i = 0; i < sizeof kFont / sizeof kFont[0]; i++) {
        if (kFont[i].c != ch)
            continue;
        for (int r = 0; r < 7; r++)
            for (int q = 0; q < 5; q++)
                if (kFont[i].r[r][q] == '#')
                    Fill(x + q * s, y + r * s, s, s, c);
        return;
    }
}

static int TextWidth(const char* t, int s) { return (int)strlen(t) * 6 * s; }

static void Text(int x, int y, int s, const char* t, uint32_t c) {
    for (; *t; t++, x += 6 * s)
        Glyph(x, y, s, *t, c);
}

static const char* kHeart[8] = { "..XX.XX..", ".XXXXXXX.", "XXXXXXXXX", "XXXXXXXXX",
                                 ".XXXXXXX.", "..XXXXX..", "...XXX...", "....X...." };

/* quarters: 0..4 of a heart filled from the left */
static void Heart(int x, int y, int s, int quarters) {
    const int fillCols = (9 * quarters + 3) / 4;
    for (int r = 0; r < 8; r++)
        for (int q = 0; q < 9; q++)
            if (kHeart[r][q] == 'X')
                Fill(x + q * s, y + r * s, s, s, q < fillCols ? RGB(232, 48, 64) : RGB(70, 26, 34));
}

static void PaintHeader(int hp, int maxhp, int coins) {
    Fill(0, 0, W, 92, RGB(8, 8, 12));
    Text(20, 16, 4, "LIFE", RGB(236, 236, 236));
    int n = maxhp / 4;
    if (n < 1)
        n = 1;
    if (n > 20)
        n = 20;
    int cell = (W - 130 - 20) / n;
    if (cell > 38)
        cell = 38; /* few hearts: keep them together instead of spreading them across the row */
    int s = cell >= 34 ? 3 : 2;
    int hw = 9 * s;
    for (int i = 0; i < n; i++) {
        int q = hp - 4 * i;
        q = q < 0 ? 0 : q > 4 ? 4 : q;
        Heart(130 + i * cell + (cell - hw) / 2, 14, s, q);
    }
    /* coin: gold disc + label + value */
    for (int r = -9; r <= 9; r++)
        for (int q = -9; q <= 9; q++)
            if (q * q + r * r <= 81)
                Fill(34 + q, 66 + r, 1, 1, (q * q + r * r > 56) ? RGB(168, 120, 20) : RGB(244, 196, 48));
    Text(62, 52, 4, "COIN", RGB(236, 236, 236));
    char buf[16];
    snprintf(buf, sizeof buf, "%d", coins);
    Text(62 + 4 * 24 + 14, 52, 4, buf, RGB(255, 214, 80));
    char hpbuf[24];
    snprintf(hpbuf, sizeof hpbuf, "%d/%d", hp, maxhp);
    Text(W - 20 - TextWidth(hpbuf, 3), 56, 3, hpbuf, RGB(180, 180, 190));
}

static void PaintMenu(void) {
    const int s = 3, x0 = (W - GBW * s) / 2, y0 = H - (GBH - MENU_TOP) * s;
    Fill(0, 92, W, H - 92, RGB(8, 8, 12));
    if (!sHaveMenu) {
        const char* msg = "WAITING FOR THE GAME";
        Text((W - TextWidth(msg, 4)) / 2, 260, 4, msg, RGB(200, 200, 210));
        return;
    }
    for (int y = MENU_TOP; y < GBH; y++)
        for (int x = 0; x < GBW; x++)
            Fill(x0 + x * s, y0 + (y - MENU_TOP) * s, s, s, sMenu[y * GBW + x]);
}

/* ------------------------------------------------------------ hidden game */

static void CaptureFrame(void) {
    int w, h;
    const uint32_t* f = Lr_Frame(&w, &h);
    if (w == GBW && h == GBH) {
        memcpy(sMenu, f, sizeof sMenu);
        sHaveMenu = true;
    }
}

static bool RecordLooksValid(const uint8_t* rec) {
    return rec[OFF_MAXHP] >= 8 && rec[OFF_MAXHP] % 4 == 0 && rec[OFF_HP] <= rec[OFF_MAXHP];
}

static const char *gCore, *gRom;
static const uint8_t* gSram;
static size_t gSramLen;
static int gSlot;

/* Title -> Load -> diary `slot` -> through the dialogs, into the game. Same key sequence verified in PyBoy. */
static bool BootIntoGame(const char* corePath, const char* rom, const uint8_t* sram, size_t sramLen, int slot) {
    gCore = corePath, gRom = rom, gSram = sram, gSramLen = sramLen, gSlot = slot;
    Lr_Close();
    if (!Lr_Open(corePath, rom))
        return false;
    size_t n;
    uint8_t* sr = Lr_Sram(&n);
    if (sr && sram)
        memcpy(sr, sram, sramLen < n ? sramLen : n);
    Lr_Run(400);
    Lr_Press(LR_START, 4, 120);
    Lr_Press(LR_DOWN, 4, 60); /* Start / Load -> Load */
    Lr_Press(LR_A, 4, 90);
    for (int i = 0; i < slot; i++)
        Lr_Press(LR_DOWN, 4, 40);
    Lr_Press(LR_A, 4, 120); /* diary */
    for (int i = 0; i < 6; i++)
        Lr_Press(LR_A, 4, 90);
    Lr_Run(60);
    fprintf(stderr, "[frog] core ran %lu frames, %lu video callbacks\n", Lr_FramesRun(), Lr_VideoCalls());
    size_t wn;
    uint8_t* wram = Lr_Wram(&wn);
    return wram && wn >= 0xE00 && RecordLooksValid(wram + (REC_BASE - 0xC000));
}

/* The menu fills the screen with panels; during play the bottom 16 lines are the black status bar. */
static bool FrameIsMenu(void) {
    int w, h;
    const uint32_t* f = Lr_Frame(&w, &h);
    if (w != GBW || h != GBH)
        return false;
    int dark = 0, total = 0;
    for (int y = GBH - 14; y < GBH - 2; y++)
        for (int x = 0; x < GBW; x++, total++) {
            uint32_t p = f[y * GBW + x];
            if ((p & 0xFF) + ((p >> 8) & 0xFF) + ((p >> 16) & 0xFF) < 150)
                dark++;
        }
    return dark * 100 < total * 40;
}

/* Press Start (the toggle) until the menu is in the wanted state; returns false if it never gets there. */
static bool SetMenu(bool wantOpen) {
    for (int tries = 0; tries < 4; tries++) {
        if (FrameIsMenu() == wantOpen)
            return true;
        Lr_Press(LR_START, 4, wantOpen ? 100 : 70);
        if (FrameIsMenu() == wantOpen)
            return true;
        Lr_Run(60); /* animation still running: give it time before pressing again */
    }
    return FrameIsMenu() == wantOpen;
}

/* Some menu actions leave the hidden game in a message or sub-screen that Start does not leave. Back out with B/A first;
 * if that fails, start the hidden copy over (about two seconds). */
static void Unstick(void) {
    static const int seq[] = { LR_B, LR_B, LR_A, LR_B, LR_B };
    for (size_t i = 0; i < sizeof seq / sizeof seq[0]; i++)
        Lr_Press(seq[i], 4, 40);
}

static bool Restart(void) {
    fprintf(stderr, "[frog] restarting the hidden game\n");
    return BootIntoGame(gCore, gRom, gSram, gSramLen, gSlot);
}

static void RefreshMenu(const uint8_t* rec) {
    for (int attempt = 0; attempt < 3; attempt++) {
        size_t wn;
        uint8_t* wram = Lr_Wram(&wn);
        if (!wram)
            return;
        bool closed = SetMenu(false);
        if (!closed) {
            Unstick();
            closed = SetMenu(false);
        }
        if (closed) {
            Lr_Run(30);
            memcpy(wram + (REC_BASE - 0xC000), rec, REC_LEN);
            if (SetMenu(true)) {
                Lr_Run(70); /* the game fades the menu in: capture only once the palette has settled */
                CaptureFrame();
                return;
            }
        }
        fprintf(stderr, "[frog] menu stuck (attempt %d)\n", attempt + 1);
        if (attempt == 0)
            Unstick();
        else if (!Restart())
            return;
    }
}

/* ------------------------------------------------------------- touch input */
/* The bottom panel's touchscreen (fe5e0000.i2c) is read straight from evdev: EmulationStation attaches the
 * touchscreens to a different Wayland seat than SDL binds. FROG_TOUCH_DEV overrides the node. */
static int sTouchFd = -1, sTouchX, sTouchY;
static bool sTouchNew, sHaveX, sHaveY;

static void OpenTouch(void) {
    char best[128] = "";
    const char* forced = getenv("FROG_TOUCH_DEV");
    if (forced) {
        snprintf(best, sizeof best, "%s", forced);
    } else {
        for (int i = 0; i < 32; i++) {
            char path[256], name[128] = "", link[512];
            snprintf(path, sizeof path, "/sys/class/input/event%d/device/name", i);
            FILE* f = fopen(path, "r");
            if (!f)
                continue;
            if (fgets(name, sizeof name, f) && strstr(name, "Goodix")) {
                snprintf(path, sizeof path, "/sys/class/input/event%d/device", i);
                ssize_t n = readlink(path, link, sizeof link - 1);
                snprintf(best, sizeof best, "/dev/input/event%d", i);
                if (n > 0) {
                    link[n] = 0;
                    if (strstr(link, "fe5e0000")) { /* the bottom panel */
                        fclose(f);
                        break;
                    }
                }
            }
            fclose(f);
        }
    }
    if (best[0]) {
        sTouchFd = open(best, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        fprintf(stderr, "[frog] touch: %s (%s)\n", best, sTouchFd >= 0 ? "ok" : "cannot open");
    }
}

/* Returns true once per finger-down with its position in panel pixels (0..639, 0..479). */
static bool PollTap(int* x, int* y) {
    struct input_event ev[32];
    ssize_t n;
    while (sTouchFd >= 0 && (n = read(sTouchFd, ev, sizeof ev)) > 0) {
        for (size_t i = 0; i < (size_t)n / sizeof ev[0]; i++) {
            if (ev[i].type == EV_ABS) {
                if (ev[i].code == ABS_MT_POSITION_X || ev[i].code == ABS_X) {
                    sTouchX = ev[i].value;
                    sHaveX = true;
                } else if (ev[i].code == ABS_MT_POSITION_Y || ev[i].code == ABS_Y) {
                    sTouchY = ev[i].value;
                    sHaveY = true;
                } else if (ev[i].code == ABS_MT_TRACKING_ID && ev[i].value >= 0) {
                    /* keep the last position: the kernel only reports an axis when it changed, so a tap at the same
                     * height as the previous one carries no Y at all */
                    sTouchNew = true;
                }
            } else if (ev[i].type == EV_KEY && ev[i].code == BTN_TOUCH && ev[i].value == 1) {
                sTouchNew = true;
            } else if (ev[i].type == EV_SYN && ev[i].code == SYN_REPORT && sTouchNew && sHaveX && sHaveY) {
                sTouchNew = false;
                *x = sTouchX;
                *y = sTouchY;
                return true;
            }
        }
    }
    return false;
}

/* ---------------------------------------------------- tap -> menu action */
/* SELECT grid of the pause menu, in the 160x144 frame: 3 rows x 4 columns of 16x16 icons (counts sit under them). */
static const int kColX[4] = { 64, 88, 112, 136 };
static const int kRowY[3] = { 56, 80, 112 };

static bool CellFromTap(int tx, int ty, int* row, int* col) {
    const int s = 3, x0 = (W - GBW * s) / 2, y0 = H - (GBH - MENU_TOP) * s;
    if (tx < x0 || ty < y0)
        return false;
    int gx = (tx - x0) / s, gy = (ty - y0) / s + MENU_TOP;
    for (int c = 0; c < 4; c++)
        for (int r = 0; r < 3; r++) {
            int top = kRowY[r] - 4, bottom = (r < 2 ? kRowY[r + 1] : GBH) - 4; /* the count under an icon belongs to it */
            if (gx >= kColX[c] - 4 && gx < kColX[c] + 20 && gy >= top && gy < bottom) {
                *row = r;
                *col = c;
                return true;
            }
        }
    return false;
}

/* Heart row of the live game's HUD (window tilemap, from the 3rd tile): 4B full, 4C/4D/4E partial, 4F empty. The
 * game redraws it on its own events; after a tap changed life, bring it in line with the record. */
static void FixLiveHud(int hp, int maxhp) {
    uint8_t lcdc = 0, tiles[24];
    if (!RaClient_Read(0xFF40, &lcdc, 1))
        return;
    const uint32_t map = (lcdc & 0x40) ? 0x9C00 : 0x9800;
    int n = maxhp / 4 > 20 ? 20 : maxhp / 4;
    static const uint8_t kTile[5] = { 0x4F, 0x4E, 0x4D, 0x4C, 0x4B };
    for (int i = 0; i < n; i++) {
        int q = hp - 4 * i;
        tiles[i] = kTile[q < 0 ? 0 : q > 4 ? 4 : q];
    }
    RaClient_Write(map + 2, tiles, (size_t)n);
}

static const uint8_t* HiddenRecord(void) {
    size_t wn;
    uint8_t* w = Lr_Wram(&wn);
    return w ? w + (REC_BASE - 0xC000) : NULL;
}

/* Do what pressing A on that cell does in the real pause menu, in the hidden copy, then apply the resulting changes
 * of the save record to the live game. Returns true if anything was written. */
static bool TapCell(int row, int col) {
    uint8_t rec0[REC_LEN], live[REC_LEN];
    if (!RaClient_Read(REC_BASE, rec0, REC_LEN) || !RecordLooksValid(rec0)) {
        fprintf(stderr, "[frog] tap r%d c%d: live record unavailable\n", row, col);
        return false;
    }
    RefreshMenu(rec0); /* hidden copy = the live record, menu freshly opened with the cursor on the first cell */
    for (int i = 0; i < col; i++)
        Lr_Press(LR_RIGHT, 4, 10);
    for (int i = 0; i < row; i++)
        Lr_Press(LR_DOWN, 4, 10);
    Lr_Press(LR_A, 4, 40);
    Lr_Run(100); /* the action (and the menu closing, for items that are used up) */
    const uint8_t* after = HiddenRecord();
    if (!after || !RaClient_Read(REC_BASE, live, REC_LEN))
        return false;
    int writes = 0, newHp = live[OFF_HP];
    bool hpChanged = false;
    /* only bytes the action changed, and only where the live game has not moved on since we looked; the first four
     * bytes are the position, which the hidden copy never owns */
    for (int i = 4; i < (int)REC_LEN;) {
        if (after[i] == rec0[i] || live[i] != rec0[i]) {
            i++;
            continue;
        }
        int j = i;
        while (j < (int)REC_LEN && after[j] != rec0[j] && live[j] == rec0[j])
            j++;
        if (RaClient_Write(REC_BASE + (uint32_t)i, after + i, (size_t)(j - i))) {
            fprintf(stderr, "[frog] tap r%d c%d: +0x%02x (%d byte%s) %02x -> %02x\n", row, col, i, j - i, j - i > 1 ? "s" : "",
                    rec0[i], after[i]);
            writes += j - i;
            if (i <= OFF_HP && OFF_HP < j) {
                hpChanged = true;
                newHp = after[OFF_HP];
            }
        }
        i = j;
    }
    if (hpChanged)
        FixLiveHud(newHp, live[OFF_MAXHP]);
    if (!writes)
        fprintf(stderr, "[frog] tap r%d c%d: nothing changed\n", row, col);
    return writes > 0;
}

/* ------------------------------------------------------------------ main */
static uint8_t* ReadFile(const char* path, size_t* n) {
    FILE* f = fopen(path, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    *n = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* b = malloc(*n);
    if (b && fread(b, 1, *n, f) != *n) {
        free(b);
        b = NULL;
    }
    fclose(f);
    return b;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: frog-bottom <rom.gb> [save.srm]\n");
        return 2;
    }
    const char* rom = argv[1];
    char sramPath[1024];
    if (argc > 2) {
        snprintf(sramPath, sizeof sramPath, "%s", argv[2]);
    } else {
        snprintf(sramPath, sizeof sramPath, "%s", rom);
        char* dot = strrchr(sramPath, '.');
        if (dot)
            *dot = 0;
        strncat(sramPath, ".srm", sizeof sramPath - strlen(sramPath) - 1);
    }
    const char* corePath = getenv("FROG_CORE") ? getenv("FROG_CORE") : "/usr/lib/libretro/gambatte_libretro.so";

    size_t sramLen = 0;
    uint8_t* sram = ReadFile(sramPath, &sramLen);
    if (!sram && argc <= 2) { /* no .srm yet: a SameBoy-style .sav next to the ROM will do for the hidden copy */
        char alt[1024];
        snprintf(alt, sizeof alt, "%s", sramPath);
        char* dot = strrchr(alt, '.');
        if (dot) {
            strcpy(dot, ".sav");
            sram = ReadFile(alt, &sramLen);
            if (sram)
                snprintf(sramPath, sizeof sramPath, "%s", alt);
        }
    }
    fprintf(stderr, "[frog] rom=%s save=%s (%s)\n", rom, sramPath, sram ? "loaded" : "missing");

    bool ready = false;
    for (int slot = 0; slot < 3 && !ready; slot++) {
        ready = BootIntoGame(corePath, rom, sram, sramLen, slot);
        fprintf(stderr, "[frog] hidden game, diary slot %d: %s\n", slot + 1, ready ? "in game" : "not in game");
    }
    if (!ready) {
        fprintf(stderr, "[frog] could not get the hidden game into play; showing live LIFE/COIN only\n");
    }
    if (getenv("FROG_DUMP_STATE")) { /* QA helper: write a state of the hosted game in play, then exit */
        bool ok = ready && Lr_SaveState(getenv("FROG_DUMP_STATE"));
        fprintf(stderr, "[frog] state dump to %s: %s\n", getenv("FROG_DUMP_STATE"), ok ? "ok" : "failed");
        Lr_Close();
        return ok ? 0 : 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    const int displays = SDL_GetNumVideoDisplays();
    const bool windowed = getenv("FROG_WINDOWED") != NULL || displays < 2;
    SDL_Window* win = SDL_CreateWindow("Frog - Bottom Screen", SDL_WINDOWPOS_UNDEFINED_DISPLAY(displays > 1 ? 1 : 0),
                                       SDL_WINDOWPOS_UNDEFINED_DISPLAY(displays > 1 ? 1 : 0), W, H,
                                       windowed ? SDL_WINDOW_RESIZABLE : (SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_BORDERLESS));
    SDL_Renderer* ren = win ? SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED) : NULL;
    if (win && !ren)
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    SDL_Texture* tex = ren ? SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, W, H) : NULL;
    if (!tex) {
        fprintf(stderr, "window/renderer failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_ShowCursor(SDL_DISABLE);

    OpenTouch();
    if (!RaClient_Open("127.0.0.1", 55355)) {
        fprintf(stderr, "[frog] cannot open the RetroArch command socket\n");
        return 1;
    }

    uint8_t rec[REC_LEN], lastRec[REC_LEN], key[REC_LEN], lastKey[REC_LEN];
    memset(lastKey, 0xAA, sizeof lastKey);
    bool connected = false, dirty = true, pendingRefresh = false;
    int hp = 0, maxhp = 0, coins = 0;
    Uint32 lastPoll = 0, changedAt = 0, lastOk = SDL_GetTicks(), start = lastOk;
    unsigned polls = 0;
    SDL_Event e;
    bool quit = false;
    while (!quit) {
        while (SDL_PollEvent(&e))
            if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE))
                quit = true;
        const Uint32 now = SDL_GetTicks();
        if (now - lastPoll >= 100) {
            lastPoll = now;
            if (RaClient_Read(REC_BASE, rec, REC_LEN) && RecordLooksValid(rec)) {
                if (!connected) {
                    fprintf(stderr, "[frog] connected to RetroArch\n");
                    connected = true;
                }
                lastOk = now;
                int nhp = rec[OFF_HP], nmax = rec[OFF_MAXHP];
                int ncoin = (rec[OFF_COIN] << 16) | (rec[OFF_COIN + 1] << 8) | rec[OFF_COIN + 2];
                if (nhp != hp || nmax != maxhp || ncoin != coins) {
                    hp = nhp, maxhp = nmax, coins = ncoin;
                    dirty = true;
                }
                /* menu-relevant bytes: everything except position (+0..3), life and coin, which the header shows */
                memcpy(key, rec, REC_LEN);
                memset(key, 0, 4);
                key[OFF_HP] = 0;
                memset(key + OFF_COIN, 0, 3);
                if (memcmp(key, lastKey, REC_LEN) != 0) {
                    memcpy(lastKey, key, REC_LEN);
                    memcpy(lastRec, rec, REC_LEN);
                    changedAt = now;
                    pendingRefresh = true;
                }
                if ((++polls % 300) == 1)
                    fprintf(stderr, "[frog] hp=%d/%d coin=%d slot0=%02x%02x%02x%02x items=%02x%02x%02x%02x\n", hp, maxhp,
                            coins, rec[0], rec[1], rec[2], rec[3], rec[0x21], rec[0x22], rec[0x23], rec[0x24]);
            } else if (connected && now - lastOk > 20000) {
                fprintf(stderr, "[frog] RetroArch is gone, exiting\n");
                break;
            } else if (!connected && now - start > 120000) {
                fprintf(stderr, "[frog] RetroArch never answered, exiting\n");
                break;
            }
        }
        {   /* touch: a tap on an item does what A does on it in the pause menu */
            int tx, ty, row, col;
            bool tapped = PollTap(&tx, &ty);
            const char* test = getenv("FROG_TEST_TAP"); /* "ms,x,y": one software tap for QA */
            static bool testDone;
            if (!tapped && test && !testDone && ready && connected && now - start >= (Uint32)atoi(test)) {
                const char* c1 = strchr(test, ',');
                const char* c2 = c1 ? strchr(c1 + 1, ',') : NULL;
                if (c2) {
                    tx = atoi(c1 + 1);
                    ty = atoi(c2 + 1);
                    tapped = testDone = true;
                    fprintf(stderr, "[frog] test tap (%d,%d)\n", tx, ty);
                }
            }
            if (tapped)
                fprintf(stderr, "[frog] tap (%d,%d) ready=%d connected=%d cell=%d\n", tx, ty, ready, connected,
                        CellFromTap(tx, ty, &row, &col));
            if (tapped && ready && connected && CellFromTap(tx, ty, &row, &col)) {
                const Uint32 t0 = SDL_GetTicks();
                bool acted = TapCell(row, col);
                fprintf(stderr, "[frog] tap handled in %u ms (%lu frames run in total)\n", SDL_GetTicks() - t0, Lr_FramesRun());
                if (acted) {
                    pendingRefresh = true; /* the live record changed: redraw the menu from it */
                    changedAt = SDL_GetTicks();
                } else {
                    pendingRefresh = true;
                    memcpy(lastRec, rec, REC_LEN);
                    changedAt = SDL_GetTicks();
                }
                lastPoll = 0;
            }
        }
        if (ready && pendingRefresh && now - changedAt >= 400) { /* settled: redraw the menu from the live record */
            pendingRefresh = false;
            memcpy(rec, lastRec, REC_LEN);
            RefreshMenu(rec);
            dirty = true;
        }
        if (dirty) {
            if (connected)
                PaintHeader(hp, maxhp, coins);
            else
                Fill(0, 0, W, 92, RGB(8, 8, 12)); /* nothing live yet: only the waiting message below */
            PaintMenu();
            SDL_UpdateTexture(tex, NULL, sScreen, W * 4);
            SDL_RenderClear(ren);
            SDL_RenderCopy(ren, tex, NULL, NULL);
            SDL_RenderPresent(ren);
            dirty = false;
        }
        SDL_Delay(15);
    }
    RaClient_Close();
    Lr_Close();
    SDL_Quit();
    return 0;
}
