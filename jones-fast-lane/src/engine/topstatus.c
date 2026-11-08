/* Life-goals panel for a second screen (the DS top screen).
 *
 * Once a game is under way (the player's walker is in the cast), the
 * players' goals and progress are read straight from the game's player
 * objects (monGoal/monStat etc.) and drawn with the game's own fonts into a
 * 256x192 8-bit buffer with its own small palette. */
#include <stdio.h>
#include <string.h>
#include "topstatus.h"
#include "vm.h"
#include "resource.h"
#include "locpanel.h"

#define W TOPSTATUS_W
#define H TOPSTATUS_H

enum { C_BG, C_PANEL, C_TEXT, C_DIM, C_TRACK, C_GOAL, C_MON, C_HAP, C_EDU, C_CAR, C_TITLE, C_DONE, C_SEL, C_COUNT };

const uint8_t topstatus_palette[TOPSTATUS_COLORS][3] = {
    [C_BG]    = {  24,  32,  64 },
    [C_PANEL] = {  40,  52,  96 },
    [C_TEXT]  = { 240, 240, 240 },
    [C_DIM]   = { 160, 170, 200 },
    [C_TRACK] = {  16,  20,  40 },
    [C_GOAL]  = { 255, 255, 255 },
    [C_MON]   = {  80, 200,  96 },
    [C_HAP]   = { 248, 200,  64 },
    [C_EDU]   = {  96, 160, 248 },
    [C_CAR]   = { 232,  88,  80 },
    [C_TITLE] = { 248, 216, 120 },
    [C_DONE]  = { 255, 255, 160 },
    [C_SEL]   = {  72,  98, 176 },
};

static int sel_ready;
static int S_playing, S_actualName, S_cash, S_cashHi, S_elements;
static int S_goal[4], S_stat[4];
static reg_t players[4], walker, cast;
static uint32_t last_scan;
static int active;

static void init_sel(void)
{
    static const char *g[4] = { "monGoal", "hapGoal", "eduGoal", "carGoal" };
    static const char *st[4] = { "monStat", "hapStat", "eduStat", "carStat" };
    for (int i = 0; i < 4; i++) { S_goal[i] = sel_id(g[i]); S_stat[i] = sel_id(st[i]); }
    S_playing = sel_id("playing");
    S_actualName = sel_id("actualName");
    S_cash = sel_id("cash");
    S_cashHi = sel_id("cashHi");
    S_elements = sel_id("elements");
    sel_ready = 1;
}

static int still_named(reg_t o, const char *name)
{
    return o && obj_is_object(o) && !strcmp(obj_name(o), name);
}

static int in_cast(void)
{
    reg_t list = get_sel(cast, S_elements);
    int guard = 0;
    for (reg_t n = list && list >= HEAP_START ? hp16(list) : 0; n && guard++ < 200; n = hp16((uint16_t)(n + 2)))
        if (hp16((uint16_t)(n + 6)) == walker) return 1;
    return 0;
}

/* Re-resolve objects now and then (scripts can move after restart/restore). */
static void rescan(uint32_t now)
{
    if (now - last_scan < 60 && still_named(players[0], "player1") &&
        still_named(walker, "theWalker") && still_named(cast, "cast"))
        return;
    last_scan = now;
    char nm[8];
    for (int i = 0; i < 4; i++) {
        snprintf(nm, sizeof nm, "player%d", i + 1);
        if (!still_named(players[i], nm)) players[i] = vm_find_object(nm);
    }
    if (!still_named(walker, "theWalker")) walker = vm_find_object("theWalker");
    if (!still_named(cast, "cast")) cast = vm_find_object("cast");
}

/* ------------------------------------------------------------ drawing */

static uint8_t *buf;

static void fill(int x, int y, int w, int h, uint8_t c)
{
    for (int j = y; j < y + h; j++) {
        if (j < 0 || j >= H) continue;
        for (int i = x; i < x + w; i++)
            if (i >= 0 && i < W) buf[j * W + i] = c;
    }
}

static const uint8_t *font(int n)
{
    uint16_t sz;
    const uint8_t *f = res_load(RES_FONT, n, &sz);
    if (f) res_unlock(RES_FONT, n);   /* stays cached; re-fetched each draw */
    return f;
}

static int text_w(const uint8_t *f, const char *s)
{
    int w = 0, n = f[2] | f[3] << 8;
    for (; *s; s++) {
        uint8_t ch = (uint8_t)*s;
        if (ch < n) w += f[f[6 + ch * 2] | f[7 + ch * 2] << 8];
    }
    return w;
}

static int text(const uint8_t *f, int x, int y, const char *s, uint8_t c)
{
    int n = f[2] | f[3] << 8;
    for (; *s; s++) {
        uint8_t ch = (uint8_t)*s;
        if (ch >= n) continue;
        const uint8_t *g = f + (f[6 + ch * 2] | f[7 + ch * 2] << 8);
        int gw = g[0], gh = g[1], bpr = (gw + 7) / 8;
        for (int j = 0; j < gh; j++)
            for (int i = 0; i < gw; i++)
                if (g[2 + j * bpr + (i >> 3)] & (0x80 >> (i & 7))) {
                    int px = x + i, py = y + j;
                    if (px >= 0 && px < W && py >= 0 && py < H) buf[py * W + px] = c;
                }
        x += gw;
    }
    return x;
}

static void bar(const uint8_t *f, int x, int y, int w, int h, const char *label, int stat, int goal, uint8_t col)
{
    if (stat < 0) stat = 0;
    if (stat > 100) stat = 100;
    if (goal < 0) goal = 0;
    if (goal > 100) goal = 100;
    int lw = 66;
    text(f, x, y + (h - (f[4] | f[5] << 8)) / 2, label, C_DIM);
    int bx = x + lw, bw = w - lw - 34;
    fill(bx, y, bw, h, C_TRACK);
    fill(bx, y, bw * stat / 100, h, col);
    int gx = bx + bw * goal / 100;
    fill(gx - 1, y - 1, 2, h + 2, C_GOAL);
    char s[16];
    snprintf(s, sizeof s, "%d/%d", stat, goal);
    text(f, bx + bw + 4, y + (h - (f[4] | f[5] << 8)) / 2, s, stat >= goal ? C_DONE : C_TEXT);
}

static void draw(int nplayers, const int *idx)
{
    const uint8_t *small = font(0), *big = font(1);
    fill(0, 0, W, H, C_BG);
    if (!small || !big) return;
    int area = H;
    const char *title = "LIFE GOALS";
    text(big, (W - text_w(big, title)) / 2, 3, title, C_TITLE);
    static const char *labels[4] = { "Wealth", "Happiness", "Education", "Career" };
    static const uint8_t cols[4] = { C_MON, C_HAP, C_EDU, C_CAR };
    int top = 19, avail = area - top - 2;
    int ph = avail / (nplayers ? nplayers : 1);
    int barh = nplayers == 1 ? (area < H ? 9 : 12) : nplayers == 2 ? (area < H ? 6 : 9) : area < H ? 4 : 6;
    int gap = (ph - 12 - 4 * barh) / 5;
    if (gap < 1) gap = 1;
    for (int k = 0; k < nplayers; k++) {
        reg_t p = players[idx[k]];
        int y = top + k * ph;
        fill(4, y, W - 8, ph - 3, C_PANEL);
        reg_t nm = get_sel(p, S_actualName);
        char name[40];
        snprintf(name, sizeof name, "%s", nm >= HEAP_START ? hstr(nm) : "Player");
        text(small, 9, y + 2, name, C_TEXT);
        uint32_t cash = (uint16_t)get_sel(p, S_cash) | (uint32_t)(uint16_t)get_sel(p, S_cashHi) << 16;
        char cs[24];
        snprintf(cs, sizeof cs, "$%lu", (unsigned long)cash);
        text(small, W - 9 - text_w(small, cs), y + 2, cs, C_TITLE);
        int by = y + 12 + gap;
        for (int i = 0; i < 4; i++) {
            bar(small, 9, by, W - 18, barh, labels[i], (int16_t)get_sel(p, S_stat[i]),
                (int16_t)get_sel(p, S_goal[i]), cols[i]);
            by += barh + gap;
        }
    }
}

/* Location panel: the open store/office redrawn in the same style. */
static void wrap(const uint8_t *f, int x, int *y, int w, const char *s, uint8_t c, int fh, int ymax)
{
    char line[96];
    size_t n = 0;
    const char *p = s;
    while (*p && *y + fh <= ymax) {
        while (*p == ' ') p++;
        n = 0;
        const char *last_break = NULL;
        size_t last_n = 0;
        line[0] = 0;
        for (const char *q = p; *q && n + 1 < sizeof line; q++) {
            line[n++] = *q; line[n] = 0;
            if (*q == ' ') { last_break = q; last_n = n - 1; }
            if (text_w(f, line) > w) {
                if (last_break) { line[last_n] = 0; p = last_break + 1; }
                else { line[n - 1] = 0; p = q; }
                goto emit;
            }
        }
        p += strlen(p);
    emit:
        text(f, x, *y, line, c);
        *y += fh;
    }
}

static void draw_location(void)
{
    const uint8_t *small = font(0), *big = font(1);
    fill(0, 0, W, H, C_BG);
    if (!small || !big) return;
    int fh = small[4] | small[5] << 8;
    /* header: location name, cash top right (the calculator) */
    const char *cash = locpanel_cash();
    int cw = cash[0] ? text_w(small, cash) + 12 : 0;
    if (cw) {
        fill(W - 4 - cw, 3, cw, fh + 4, C_TRACK);
        text(small, W - 4 - cw + 6, 5, cash, C_TITLE);
    }
    const char *title = locpanel_title();
    int tw = text_w(big, title), avail = W - 8 - cw;
    text(big, 4 + (avail > tw ? (avail - tw) / 2 : 0), 3, title, C_TITLE);

    const LocEntry *e;
    int n = locpanel_entries(&e), sel = locpanel_selected();
    int top = 19, bottom = H - 3;
    fill(4, top, W - 8, bottom - top, C_PANEL);
    /* buttons along the bottom of the panel */
    int bh = fh + 5, nb = 0, bw_total = 0;
    for (int i = 0; i < n; i++) if (e[i].kind == LOC_BUTTON) { nb++; bw_total += text_w(small, e[i].label) + 14; }
    int rows_bottom = bottom - (nb ? bh + 6 : 2);
    if (nb) {
        int gap = nb > 1 ? (W - 16 - bw_total) / (nb - 1) : 0;
        if (gap > 10) gap = 10;
        if (gap < 2) gap = 2;
        int total = bw_total + gap * (nb - 1);
        int x = (W - total) / 2, y = bottom - bh - 3;
        for (int i = 0; i < n; i++) {
            if (e[i].kind != LOC_BUTTON) continue;
            int bw = text_w(small, e[i].label) + 14;
            fill(x, y, bw, bh, i == sel ? C_SEL : C_TRACK);
            if (i == sel) { fill(x, y, bw, 1, C_GOAL); fill(x, y + bh - 1, bw, 1, C_GOAL); }
            text(small, x + 7, y + 3, e[i].label, !e[i].enabled ? C_DIM : i == sel ? C_DONE : C_TEXT);
            x += bw + gap;
        }
    }
    /* rows: messages wrap, items get a price column and the highlight */
    int y = top + 4, rh = fh + 4;
    for (int i = 0; i < n && y + fh <= rows_bottom; i++) {
        if (e[i].kind == LOC_BUTTON) continue;
        if (e[i].kind == LOC_MESSAGE) {
            wrap(small, 10, &y, W - 20, e[i].label, C_DIM, fh, rows_bottom);
            y += 2;
            continue;
        }
        if (i == sel) fill(6, y - 2, W - 12, rh, C_SEL);
        uint8_t tc = !e[i].enabled ? C_DIM : i == sel ? C_DONE : C_TEXT;
        if (i == sel) text(small, 9, y, ">", C_GOAL);
        text(small, 17, y, e[i].label, tc);
        if (e[i].price[0]) text(small, W - 12 - text_w(small, e[i].price), y, e[i].price, i == sel ? C_DONE : C_TITLE);
        y += rh;
    }
}

/* ------------------------------------------------------------ public */

int topstatus_update(uint8_t *out, uint32_t now)
{
    if (!sel_ready) init_sel();
    rescan(now);
    if (!still_named(players[0], "player1") || !still_named(cast, "cast")) {
        active = 0;
        return 0;
    }
    if (!active && still_named(walker, "theWalker") && in_cast()) active = 1;
    if (!active) return 0;

    int idx[4], n = 0;
    for (int i = 0; i < 4; i++)
        if (still_named(players[i], i == 0 ? "player1" : i == 1 ? "player2" : i == 2 ? "player3" : "player4") &&
            get_sel(players[i], S_playing))
            idx[n++] = i;
    if (!n) { active = 0; return 0; }

    /* redraw only when something shown changed */
    static uint32_t last_sig = 0xffffffff;
    if (locpanel_active()) {
        uint32_t ls = locpanel_signature() * 2246822519u + 1;
        if (ls == last_sig && out == buf) return 1;
        last_sig = ls;
        buf = out;
        draw_location();
        return 2;
    }
    uint32_t sig = (uint32_t)n * 2654435761u;
    for (int k = 0; k < n; k++) {
        reg_t p = players[idx[k]];
        for (int i = 0; i < 4; i++)
            sig = sig * 31 + get_sel(p, S_goal[i]) * 7 + get_sel(p, S_stat[i]);
        sig = sig * 31 + get_sel(p, S_cash) + (get_sel(p, S_cashHi) << 8) + get_sel(p, S_actualName);
    }
    if (sig == last_sig && out == buf) return 1;
    last_sig = sig;
    buf = out;
    draw(n, idx);
    return 2;
}
