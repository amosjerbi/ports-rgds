/* 320x200 -> 256x192 for the bottom screen, kept sharp without losing the game's thin text.
 *
 * Built as ARM code in ITCM (the Makefile's *.itcm.c rule; calico's ds9.ld puts *.itcm.* objects there).
 *
 * Fitting 320 columns into 256 means one source pixel in every 5 has to go (and one row in every 25).
 * The hardware's affine scaler always drops the same column, so 1-pixel strokes vanished; an area filter
 * kept them but blurred everything. Here the pixel that goes is chosen per pixel: one that repeats a
 * neighbour (dropping it loses nothing). Only where nothing nearby repeats -- fine detail packed tight --
 * are the two candidates blended. Text on a plain background stays fully sharp.
 *
 *   across, each group of 5 columns a b c d e -> 4 outputs: a, then two of b c d, then e
 *   down, 25 rows -> 24: the nearest row; at the one row per group that has no output of its own, it
 *   replaces whichever neighbour row repeats there (or is blended in)
 *
 * Colours are in a "spread" form: the RGB15 channels moved apart (red bits 0-4, green 11-15, blue 22-26)
 * so a blend is one 32-bit multiply-add; equal spread values are equal colours.
 */
#include <string.h>
#include "scale.h"

#define FIELDS 0x07C0F81Fu          /* the three 5-bit channels in spread form */

static uint32_t pal_s[256];         /* palette in spread form */
static uint8_t prev[SCALE_SRC_W * SCALE_SRC_H];
static uint8_t row_dirty[SCALE_SRC_H];
static int full_redraw = 1;

#define SLOTS 6
static uint32_t hrow[SLOTS][SCALE_DST_W]; /* horizontally reduced source rows (spread form) */
static int hrow_src[SLOTS];
static unsigned hrow_age[SLOTS], hrow_clock;

/* output row k shows source row v_row[k]; v_fix[k]: 0 plain, 1 = the row after it (v_row+1) has no output
 * row and may replace this one, 2 = the row before it (v_row-1) has none and may replace this one */
static uint8_t v_row[SCALE_DST_H], v_fix[SCALE_DST_H];
static int tables_ready;

static void make_tables(void)
{
    static uint8_t used[SCALE_SRC_H];
    memset(used, 0, sizeof used);
    for (int k = 0; k < SCALE_DST_H; k++) {
        int r = ((2 * k + 1) * SCALE_SRC_H) / (2 * SCALE_DST_H);   /* nearest: centre of output row k */
        v_row[k] = (uint8_t)r; v_fix[k] = 0; used[r] = 1;
    }
    for (int k = 0; k < SCALE_DST_H; k++) {
        int r = v_row[k];
        /* the fixes read rows j-2..j+1 around the row j with no output (j+2 is clamped) */
        if (r >= 1 && r + 2 < SCALE_SRC_H && !used[r + 1]) v_fix[k] = 1;      /* j = r+1 */
        if (r >= 3 && r < SCALE_SRC_H && !used[r - 1]) v_fix[k] = 2;          /* j = r-1 */
    }
    tables_ready = 1;
}

static inline uint32_t spread(uint16_t c)
{
    return (c & 0x1Fu) | ((c & 0x3E0u) << 6) | ((c & 0x7C00u) << 12);
}

static inline uint32_t half(uint32_t x, uint32_t y)   /* 50/50 blend in spread form */
{
    return ((x + y + 0x00401001u) >> 1) & FIELDS;
}

void scale_set_palette(const uint16_t *rgb15)
{
    for (int i = 0; i < 256; i++) pal_s[i] = spread(rgb15[i]);
    full_redraw = 1;
}

void scale_invalidate(void) { full_redraw = 1; }

/* one source row, 320 indices -> 256 colours in spread form */
static void hreduce(const uint8_t *s, uint32_t *o)
{
    for (int x = 0; x < SCALE_SRC_W; x += 5, s += 5, o += 4) {
        unsigned ia = s[0], ib = s[1], ic = s[2], id = s[3], ie = s[4];
        o[0] = pal_s[ia];
        o[3] = pal_s[ie];
        if (ic == ib || ic == id) {             /* c repeats a neighbour: drop it */
            o[1] = pal_s[ib]; o[2] = pal_s[id];
        } else if (ib == ia) {                  /* b repeats a: drop b */
            o[1] = pal_s[ic]; o[2] = pal_s[id];
        } else if (id == ie) {                  /* d repeats e: drop d */
            o[1] = pal_s[ib]; o[2] = pal_s[ic];
        } else {                                /* all distinct: share c out */
            uint32_t c = pal_s[ic];
            o[1] = half(pal_s[ib], c); o[2] = half(c, pal_s[id]);
        }
    }
}

static const uint32_t *get_hrow(const uint8_t *src, int r)
{
    int oldest = 0;
    for (int i = 0; i < SLOTS; i++) {
        if (hrow_src[i] == r) { hrow_age[i] = ++hrow_clock; return hrow[i]; }
        if (hrow_age[i] < hrow_age[oldest]) oldest = i;
    }
    hreduce(src + r * SCALE_SRC_W, hrow[oldest]);
    hrow_src[oldest] = r;
    hrow_age[oldest] = ++hrow_clock;
    return hrow[oldest];
}

static inline uint32_t pack(uint32_t v)
{
    return (v & 0x1Fu) | ((v >> 6) & 0x3E0u) | ((v >> 12) & 0x7C00u) | 0x8000u;
}

void scale_frame(const uint8_t *src, uint16_t *dst)
{
    if (!tables_ready) make_tables();
    int any = 0;
    if (full_redraw) {
        memset(row_dirty, 1, sizeof row_dirty);
        memcpy(prev, src, sizeof prev);
        full_redraw = 0;
        any = 1;
    } else {
        for (int y = 0; y < SCALE_SRC_H; y++) {
            const uint8_t *s = src + y * SCALE_SRC_W;
            uint8_t *p = prev + y * SCALE_SRC_W;
            if (memcmp(s, p, SCALE_SRC_W)) { memcpy(p, s, SCALE_SRC_W); row_dirty[y] = 1; any = 1; }
            else row_dirty[y] = 0;
        }
    }
    if (!any) return;
    for (int i = 0; i < SLOTS; i++) { hrow_src[i] = -1; hrow_age[i] = 0; }   /* rows may have changed */

    for (int k = 0; k < SCALE_DST_H; k++) {
        int r = v_row[k], fix = v_fix[k];
        /* rows this output looks at: r, and for a fix r-2..r+2 */
        int lo = fix ? r - 2 : r, hi = fix ? r + 2 : r, d = 0;
        if (lo < 0) lo = 0;
        if (hi >= SCALE_SRC_H) hi = SCALE_SRC_H - 1;
        for (int y = lo; y <= hi; y++) d |= row_dirty[y];
        if (!d) continue;
        uint32_t *o = (uint32_t *)(dst + k * 256);   /* VRAM: written two pixels at a time */
        if (!fix) {
            const uint32_t *a = get_hrow(src, r);
            for (int x = 0; x < SCALE_DST_W; x += 2)
                *o++ = pack(a[x]) | (pack(a[x + 1]) << 16);
            continue;
        }
        /* the row j with no output of its own, between P (row j-1) and N (row j+1); Q = j-2, M = j+2.
         * Output k shows P (fix 1) or N (fix 2); the same decision is made from both sides so D lands in
         * exactly one of them: D repeats P or N -> drop it; else P repeats Q -> D replaces P;
         * else N repeats M -> D replaces N; else D is blended into both. */
        int j = fix == 1 ? r + 1 : r - 1;
        const uint32_t *Q = get_hrow(src, j - 2), *P = get_hrow(src, j - 1), *D = get_hrow(src, j),
                       *N = get_hrow(src, j + 1), *M = get_hrow(src, j + 2 < SCALE_SRC_H ? j + 2 : j + 1);
        uint32_t px[2];
        for (int x = 0; x < SCALE_DST_W; x += 2) {
            for (int i = 0; i < 2; i++) {
                uint32_t p = P[x + i], dd = D[x + i], n = N[x + i], self = fix == 1 ? p : n, v;
                if (dd == p || dd == n) v = self;
                else if (p == Q[x + i]) v = fix == 1 ? dd : n;
                else if (n == M[x + i]) v = fix == 2 ? dd : p;
                else v = half(self, dd);
                px[i] = v;
            }
            *o++ = pack(px[0]) | (pack(px[1]) << 16);
        }
    }
}
