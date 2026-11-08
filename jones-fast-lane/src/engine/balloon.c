/* Speech balloons for the CD release. The floppy release prints each line in
 * a "bubbleWindow" beside the talking portrait; the CD release speaks the
 * line instead and draws nothing. This module redraws that balloon on top of
 * the finished frame (never into the game's own screens), using the same
 * pieces as the floppy: view 0 loop 6 (corners 12-15, tails 0-2 / 6-8), fill
 * colour 121, black outline, font 1 text in colour 27, 100 pixels wide. */
#include <string.h>
#include "balloon.h"
#include "subtitle.h"
#include "gfx.h"
#include "resource.h"
#include "vm.h"

#define FILL    121
#define INK     27
#define EDGE    0
#define TEXT_W  100
#define PAD_X   5
#define PAD_Y   8
#define CORNER  12

static int talker_view = -1;            /* the portrait view of theTalker */
static int ax = -1, ay, aw, ah;         /* where that view was last drawn */
static const char *shown;               /* text the anchor belongs to */

void balloon_cel_drawn(int view, int x, int y, int w, int h)
{
    if (view != talker_view || !shown) return;
    ax = x; ay = y; aw = w; ah = h;
}

static void find_talker(void)
{
    static int sel_view = -2;
    if (sel_view == -2) sel_view = sel_id("view");
    talker_view = -1;
    if (sel_view < 0) return;
    reg_t t = vm_find_object("theTalker");
    if (t) talker_view = (int16_t)get_sel(t, sel_view);
}

/* ---------------------------------------------------------------- drawing */

static uint8_t *dst;

static void px(int x, int y, uint8_t c)
{
    if ((unsigned)x < SCREEN_W && (unsigned)y < SCREEN_H) dst[y * SCREEN_W + x] = c;
}

static void box(int x, int y, int w, int h, uint8_t c)
{
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++) px(i, j, c);
}

static uint8_t celbuf[64 * 64];

static void cel(const uint8_t *view, int c, int x, int y)
{
    CelInfo ci;
    int mir = 0;
    if (!view || !view_get_cel(view, 6, c, &ci, &mir)) return;
    if ((uint32_t)ci.width * ci.height > sizeof celbuf) return;
    cel_decode(&ci, celbuf);
    for (int j = 0; j < ci.height; j++)
        for (int i = 0; i < ci.width; i++) {
            uint8_t v = celbuf[j * ci.width + i];
            if (v != ci.clear_key) px(x + i, y + j, v);
        }
}

static void glyphs(const uint8_t *f, const char *s, int n, int x, int y)
{
    int count = f[2] | f[3] << 8;
    for (int k = 0; k < n; k++) {
        int ch = (uint8_t)s[k];
        if (ch >= count) continue;
        const uint8_t *g = f + (f[6 + ch * 2] | f[7 + ch * 2] << 8);
        int w = g[0], h = g[1], bpr = (w + 7) / 8;
        for (int j = 0; j < h; j++)
            for (int i = 0; i < w; i++)
                if (g[2 + j * bpr + (i >> 3)] & (0x80 >> (i & 7))) px(x + i, y + j, INK);
        x += w;
    }
}

/* Break `s` into lines no wider than TEXT_W; returns the count. */
#define MAXLINES 12
static int wrap(const uint8_t *f, const char *s, const char **start, int *len)
{
    int n = 0;
    while (*s && n < MAXLINES) {
        while (*s == ' ') s++;
        if (!*s) break;
        int i = 0, w = 0, brk = -1;
        while (s[i] && s[i] != '\n') {
            int cw = font_char_width(f, (uint8_t)s[i]);
            if (w + cw > TEXT_W) break;
            if (s[i] == ' ') brk = i;
            w += cw;
            i++;
        }
        if (s[i] && s[i] != '\n' && brk > 0) i = brk;
        if (i == 0) i = 1;
        start[n] = s;
        len[n++] = i;
        s += i;
        if (*s == '\n') s++;
    }
    return n;
}

int balloon_overlay(uint8_t *frame, uint32_t now)
{
    const char *s = subtitle_current(now);
    if (!s) return 0;
    uint16_t sz;
    const uint8_t *f = res_load(RES_FONT, 1, &sz);
    const uint8_t *v = res_load(RES_VIEW, 0, &sz);
    if (!f) return 0;
    const char *starts[MAXLINES];
    int lens[MAXLINES];
    int n = wrap(f, s, starts, lens);
    if (!n) return 0;
    int fh = font_height(f);
    int w = TEXT_W + 2 * PAD_X, h = n * fh + 2 * PAD_Y;
    if (h < 2 * CORNER + 4) h = 2 * CORNER + 4;

    /* placement: beside the portrait, like the floppy (left of a portrait
     * on the right half, right of one on the left); centred low otherwise */
    int x, y, tail = -1, tx = 0, ty = 0;
    if (ax >= 0) {
        y = ay + 23;
        if (ax + aw / 2 >= SCREEN_W / 2) { x = ax - 10 - w; tail = 2; }
        else { x = ax + aw + 10; tail = 6; }
        if (x < 2) x = 2;
        if (x + w > SCREEN_W - 2) x = SCREEN_W - 2 - w;
        if (y + h > SCREEN_H - 2) y = SCREEN_H - 2 - h;
        if (y < 2) y = 2;
        ty = y + CORNER;
        tx = tail == 2 ? x + w - 1 : x - 15;
    } else {
        x = (SCREEN_W - w) / 2;
        y = SCREEN_H - 8 - h;
    }

    dst = frame;
    /* body: the floppy's five fills, corner cels, then the outline */
    box(x + CORNER, y, w - 2 * CORNER, h, FILL);
    box(x, y + CORNER, w, h - 2 * CORNER, FILL);
    cel(v, 12, x, y);
    cel(v, 13, x + w - CORNER, y);
    cel(v, 14, x, y + h - CORNER);
    cel(v, 15, x + w - CORNER, y + h - CORNER);
    box(x + CORNER, y, w - 2 * CORNER, 1, EDGE);
    box(x + CORNER, y + h - 1, w - 2 * CORNER, 1, EDGE);
    box(x, y + CORNER, 1, h - 2 * CORNER, EDGE);
    box(x + w - 1, y + CORNER, 1, h - 2 * CORNER, EDGE);
    if (tail >= 0) cel(v, tail, tx, ty);

    int ty0 = y + (h - n * fh) / 2;
    for (int i = 0; i < n; i++) {
        const char *ln = starts[i];
        int lw = 0;
        for (int k = 0; k < lens[i]; k++) lw += font_char_width(f, (uint8_t)ln[k]);
        glyphs(f, ln, lens[i], x + (w - lw) / 2, ty0 + i * fh);
    }
    return 1;
}

uint32_t balloon_signature(uint32_t now)
{
    const char *s = subtitle_current(now);
    if (s != shown) {                    /* new line: look up who speaks */
        shown = s;
        ax = -1;
        if (s) find_talker();
    }
    if (!s) return 0;
    return (uint32_t)(uintptr_t)s * 31u + (uint32_t)(ax * 7 + ay * 131 + 1);
}
