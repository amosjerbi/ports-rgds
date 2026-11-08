/* SCI1 VGA graphics core. */
#include <string.h>
#include <stdlib.h>
#include "gfx.h"

GfxState gfx;
PicStats pic_stats;

#define WHITE 255

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

/* ------------------------------------------------------------------ setup */

void gfx_set_priority_bands(int top, int bottom)
{
    const int count = 14;
    int32_t band = ((bottom - top) * 2000) / count;
    int y;
    for (y = 0; y < top && y < 200; y++)
        gfx.priority_bands[y] = 0;
    for (y = top; y < bottom && y < 200; y++)
        gfx.priority_bands[y] = (uint8_t)(1 + ((y - top) * 2000) / band);
    for (y = bottom; y < 200; y++)
        gfx.priority_bands[y] = count;
}

static void set_priority_table(const uint8_t *data)
{
    int i = 0, inx;
    for (inx = 0; inx < 14; inx++) {
        int p = data[inx];
        while (i < p && i < 200)
            gfx.priority_bands[i++] = (uint8_t)inx;
    }
    while (i < 200)
        gfx.priority_bands[i++] = (uint8_t)inx;
}

void gfx_clear(uint8_t visual, uint8_t priority, uint8_t control)
{
    memset(gfx.visual, visual, sizeof gfx.visual);
    memset(gfx.priority, priority, sizeof gfx.priority);
    memset(gfx.control, control, sizeof gfx.control);
}

void gfx_init(void)
{
    memset(&gfx, 0, sizeof gfx);
    gfx_clear(0, 0, 0);
    /* default: black, white at 255 */
    gfx.palette[255] = (PalColor){ 1, 255, 255, 255 };
    gfx.palette_dirty = 1;
    memset(gfx.intensity, 100, sizeof gfx.intensity);
    gfx_set_priority_bands(42, 200);
}

/* SCI1 palette block: 256 mapping bytes, 4 byte timestamp, 256 x (used,r,g,b).
 * force: copy every entry; otherwise only entries flagged as used. */
void gfx_palette_from_data(const uint8_t *data, int force)
{
    const uint8_t *c = data + 260;
    for (int i = 0; i < 256; i++, c += 4) {
        if (force || c[0]) {
            gfx.palette[i].used = c[0];
            gfx.palette[i].r = c[1];
            gfx.palette[i].g = c[2];
            gfx.palette[i].b = c[3];
        }
    }
    gfx.palette_dirty = 1;
    gfx.palette_gen++;
}

/* ------------------------------------------------------------- primitives */

static inline void put_pixel(int x, int y, int mask, uint8_t color, uint8_t prio, uint8_t ctrl)
{
    if ((unsigned)x >= SCREEN_W || (unsigned)y >= SCREEN_H)
        return;
    int o = y * SCREEN_W + x;
    if (mask & MASK_VISUAL)   gfx.visual[o] = color;
    if (mask & MASK_PRIORITY) gfx.priority[o] = prio;
    if (mask & MASK_CONTROL)  gfx.control[o] = ctrl;
}

static int draw_mask(uint8_t color, uint8_t prio, uint8_t ctrl)
{
    return (color != 255 ? MASK_VISUAL : 0) | (prio != 255 ? MASK_PRIORITY : 0) |
           (ctrl != 255 ? MASK_CONTROL : 0);
}

void gfx_draw_line(int x0, int y0, int x1, int y1, uint8_t color, uint8_t prio, uint8_t ctrl)
{
    int mask = draw_mask(color, prio, ctrl);
    int dx = x1 - x0, dy = y1 - y0, stepx = 1, stepy = 1, fraction;
    if (!mask)
        return;
    if (dy < 0) { dy = -dy; stepy = -1; }
    if (dx < 0) { dx = -dx; stepx = -1; }
    dy <<= 1; dx <<= 1;
    put_pixel(x0, y0, mask, color, prio, ctrl);
    if (dx > dy) {
        fraction = dy - (dx >> 1);
        while (x0 != x1) {
            if (fraction >= 0) { y0 += stepy; fraction -= dx; }
            x0 += stepx; fraction += dy;
            put_pixel(x0, y0, mask, color, prio, ctrl);
        }
    } else {
        fraction = dx - (dy >> 1);
        while (y0 != y1) {
            if (fraction >= 0) { x0 += stepx; fraction -= dy; }
            y0 += stepy; fraction += dx;
            put_pixel(x0, y0, mask, color, prio, ctrl);
        }
    }
}

void gfx_fill_rect(int x, int y, int w, int h, uint8_t color)
{
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++)
            put_pixel(i, j, MASK_VISUAL, color, 0, 0);
}

/* ------------------------------------------------------------------- fill */

static uint16_t fill_stack[8192][2];

static inline int fill_match(int o, int match_mask, uint8_t sc, uint8_t sp, uint8_t sk)
{
    if (match_mask == MASK_VISUAL)   return gfx.visual[o] == sc;
    if (match_mask == MASK_PRIORITY) return gfx.priority[o] == sp;
    return gfx.control[o] == sk;
}

static void flood_fill(int x, int y, uint8_t color, uint8_t prio, uint8_t ctrl)
{
    int mask = draw_mask(color, prio, ctrl);
    if ((unsigned)x >= SCREEN_W || (unsigned)y >= SCREEN_H || !mask)
        return;
    int o = y * SCREEN_W + x;
    uint8_t sc = gfx.visual[o], sp = gfx.priority[o], sk = gfx.control[o];

    if (mask & MASK_VISUAL) {
        if (color == WHITE || sc != WHITE)
            return;
    } else if (mask & MASK_PRIORITY) {
        if (prio == 0 || sp != 0)
            return;
    } else if (mask & MASK_CONTROL) {
        if (ctrl == 0 || sk != 0)
            return;
    }
    if ((mask & MASK_VISUAL) && sc == color)  mask &= ~MASK_VISUAL;
    if ((mask & MASK_PRIORITY) && sp == prio) mask &= ~MASK_PRIORITY;
    if ((mask & MASK_CONTROL) && sk == ctrl)  mask &= ~MASK_CONTROL;
    if (!mask)
        return;
    int match = (mask & MASK_VISUAL) ? MASK_VISUAL :
                (mask & MASK_PRIORITY) ? MASK_PRIORITY : MASK_CONTROL;

    int sp_top = 0;
    fill_stack[sp_top][0] = (uint16_t)x;
    fill_stack[sp_top][1] = (uint16_t)y;
    sp_top++;
    while (sp_top > 0) {
        sp_top--;
        int cx = fill_stack[sp_top][0], cy = fill_stack[sp_top][1];
        int row = cy * SCREEN_W;
        if (!fill_match(row + cx, match, sc, sp, sk))
            continue;
        int l = cx, r = cx;
        while (l > 0 && fill_match(row + l - 1, match, sc, sp, sk)) l--;
        while (r < SCREEN_W - 1 && fill_match(row + r + 1, match, sc, sp, sk)) r++;
        for (int i = l; i <= r; i++)
            put_pixel(i, cy, mask, color, prio, ctrl);
        for (int dir = -1; dir <= 1; dir += 2) {
            int ny = cy + dir;
            if (ny < 0 || ny >= SCREEN_H)
                continue;
            int nrow = ny * SCREEN_W, in_run = 0;
            for (int i = l; i <= r; i++) {
                if (fill_match(nrow + i, match, sc, sp, sk)) {
                    if (!in_run && sp_top < (int)(sizeof fill_stack / sizeof *fill_stack)) {
                        fill_stack[sp_top][0] = (uint16_t)i;
                        fill_stack[sp_top][1] = (uint16_t)ny;
                        sp_top++;
                    }
                    in_run = 1;
                } else {
                    in_run = 0;
                }
            }
        }
    }
}

/* --------------------------------------------------------------- patterns */
/* TODO: exact Sierra circle/texture tables; approximation used for now. */
static void draw_pattern(int x, int y, uint8_t color, uint8_t prio, uint8_t ctrl,
                         int code, int texture)
{
    int size = code & 7;
    int mask = draw_mask(color, prio, ctrl);
    pic_stats.patterns++;
    y -= size; if (y < 0) y = 0;
    x -= size; if (x < 0) x = 0;
    int h = size * 2 + 1, w = size * 2 + 2;
    uint32_t seed = (uint32_t)texture * 2654435761u;
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            if (!(code & 0x10)) {
                int cx = 2 * i - (w - 1), cy = 2 * j - (h - 1);
                if (cx * cx + cy * cy > (h) * (h))
                    continue;
            }
            if (code & 0x20) {
                seed = seed * 1103515245u + 12345u;
                if (!((seed >> 16) & 1))
                    continue;
            }
            put_pixel(x + i, y + j, mask, color, prio, ctrl);
        }
    }
}

/* ------------------------------------------------------------------- cels */

void cel_decode(const CelInfo *cel, uint8_t *out)
{
    uint32_t total = (uint32_t)cel->width * cel->height, n = 0;
    const uint8_t *p = cel->rle;
    while (n < total) {
        uint8_t b = *p++;
        uint32_t run = b & 0x3f;
        switch (b & 0xc0) {
        case 0x40:
            run += 64;
            /* fall through */
        case 0x00:
            for (uint32_t i = 0; i < run; i++) {
                if (n + i < total) out[n + i] = p[i];
            }
            p += run;
            break;
        case 0x80:
            for (uint32_t i = 0; i < run && n + i < total; i++)
                out[n + i] = *p;
            p++;
            break;
        case 0xc0:
            for (uint32_t i = 0; i < run && n + i < total; i++)
                out[n + i] = cel->clear_key;
            break;
        }
        n += run;
    }
}

static uint8_t cel_buf[SCREEN_W * SCREEN_H];

void gfx_draw_cel_raw(const CelInfo *cel, int mirrored, int x, int y, int priority)
{
    int w = cel->width, h = cel->height;
    if ((uint32_t)w * h > sizeof cel_buf)
        return;
    cel_decode(cel, cel_buf);
    for (int j = 0; j < h; j++) {
        int sy = y + j;
        if ((unsigned)sy >= SCREEN_H)
            continue;
        for (int i = 0; i < w; i++) {
            int sx = x + i;
            if ((unsigned)sx >= SCREEN_W)
                continue;
            uint8_t c = cel_buf[j * w + (mirrored ? w - 1 - i : i)];
            if (c == cel->clear_key)
                continue;
            int o = sy * SCREEN_W + sx;
            if (priority != 0xff) {
                if (gfx.priority[o] > priority)
                    continue;
                gfx.priority[o] = (uint8_t)priority;
            }
            gfx.visual[o] = c;
        }
    }
}

/* SCI1 view: [0]=loop count, [1]=flags, [2..3]=mirror mask, [6..7]=palette
 * offset, [8..]=loop offsets. Loop: cel count, unknown, cel offsets.
 * Cel: w, h, dx(s8), dy(u8), clear key, 1 pad, RLE data. */
int view_loop_count(const uint8_t *view) { return view[0]; }

int view_cel_count(const uint8_t *view, int loop)
{
    if (loop < 0 || loop >= view[0])
        return 0;
    return rd16(view + rd16(view + 8 + loop * 2));
}

int view_get_cel(const uint8_t *view, int loop, int cel, CelInfo *out, int *mirrored)
{
    int loops = view[0];
    if (loops == 0)
        return 0;
    if (loop >= loops) loop = loops - 1;
    if (loop < 0) loop = 0;
    const uint8_t *ld = view + rd16(view + 8 + loop * 2);
    int cels = rd16(ld);
    if (cels == 0)
        return 0;
    if (cel >= cels) cel = cels - 1;
    if (cel < 0) cel = 0;
    const uint8_t *cd = view + rd16(ld + 4 + cel * 2);
    out->width = rd16(cd);
    out->height = rd16(cd + 2);
    out->dx = (int8_t)cd[4];
    out->dy = cd[5];
    out->clear_key = cd[6];
    out->rle = cd + 8;
    if (mirrored)
        *mirrored = (rd16(view + 2) >> loop) & 1;
    return 1;
}

/* Closest used system colour (SCI1 matchColor); bit 15 set on an exact match. */
static int palette_match(int r, int g, int b)
{
    int best = 0x7fff, bi = 255;
    for (int i = 0; i < 256; i++) {
        const PalColor *c = &gfx.palette[i];
        if (!c->used) continue;
        int d = abs(c->r - r) + abs(c->g - g) + abs(c->b - b);
        if (d <= best) { best = d; bi = i; if (!d) break; }
    }
    return best ? bi : (bi | 0x8000);
}

/* Non-forced SCI1 palette merge: entries the system palette does not use
 * yet are taken over; other colours are matched exactly, put in a free slot,
 * or approximated. map[] receives where each colour of `data` ended up. */
void gfx_palette_merge(const uint8_t *data, uint8_t *map)
{
    const uint8_t *c = data + 260;
    int changed = 0;
    for (int i = 0; i < 256; i++) map[i] = (uint8_t)i;
    for (int i = 1; i < 255; i++) {
        const uint8_t *n = c + 4 * i;
        if (!n[0]) continue;
        PalColor *s = &gfx.palette[i];
        if (!s->used) {
            s->used = n[0];
            if (s->r != n[1] || s->g != n[2] || s->b != n[3]) { s->r = n[1]; s->g = n[2]; s->b = n[3]; changed = 1; }
            continue;
        }
        if (s->r == n[1] && s->g == n[2] && s->b == n[3]) continue;
        int m = palette_match(n[1], n[2], n[3]);
        if (m & 0x8000) { map[i] = (uint8_t)m; continue; }
        int j;
        for (j = 1; j < 256; j++) {
            PalColor *f = &gfx.palette[j];
            if (!f->used) {
                f->used = n[0]; f->r = n[1]; f->g = n[2]; f->b = n[3];
                map[i] = (uint8_t)j;
                changed = 1;
                break;
            }
        }
        if (j == 256) { map[i] = (uint8_t)m; gfx.palette[m].used |= 0x10; }
    }
    if (changed) { gfx.palette_dirty = 1; gfx.palette_gen++; }
}

/* Merge a view's embedded palette; returns the colour map to draw it with
 * (NULL when the view has none). Cached until the palette changes. */
const uint8_t *view_apply_palette(const uint8_t *view, uint32_t size)
{
    uint16_t off = rd16(view + 6);
    if (!off || off == 0x100 || off + 1284u > size) return NULL;
    static struct { const uint8_t *view; uint32_t gen; uint8_t map[256]; } cache[8];
    static int next;
    for (int k = 0; k < 8; k++)
        if (cache[k].view == view && cache[k].gen == gfx.palette_gen) return cache[k].map;
    int k = next++ & 7;
    gfx_palette_merge(view + off, cache[k].map);
    cache[k].view = view;
    cache[k].gen = gfx.palette_gen;
    return cache[k].map;
}

void gfx_draw_cel(const uint8_t *view, int loop, int cel, int x, int y, int priority)
{
    CelInfo c;
    int mir = 0;
    if (!view_get_cel(view, loop, cel, &c, &mir))
        return;
    int dx = mir ? -c.dx : c.dx;
    int left = x - c.width / 2 + dx;
    int top = y - c.height + 1 + c.dy;
    gfx_draw_cel_raw(&c, mir, left, top, priority);
}

/* -------------------------------------------------------------------- pic */

static void abs_coords(const uint8_t *d, uint32_t *pos, int *x, int *y)
{
    uint8_t b = d[(*pos)++];
    *x = d[(*pos)++] + ((b & 0xf0) << 4);
    *y = d[(*pos)++] + ((b & 0x0f) << 8);
}

static void rel_coords(const uint8_t *d, uint32_t *pos, int *x, int *y, int mir)
{
    uint8_t b = d[(*pos)++];
    int s = mir ? -1 : 1;
    if (b & 0x80) *x -= ((b >> 4) & 7) * s; else *x += (b >> 4) * s;
    if (b & 0x08) *y -= (b & 7); else *y += (b & 7);
}

static void rel_coords_med(const uint8_t *d, uint32_t *pos, int *x, int *y, int mir)
{
    uint8_t b = d[(*pos)++];
    int s = mir ? -1 : 1;
    if (b & 0x80) *y -= (b & 0x7f); else *y += b;
    b = d[(*pos)++];
    if (b & 0x80) *x -= (128 - (b & 0x7f)) * s; else *x += b * s;
}

int gfx_draw_pic(const uint8_t *d, uint32_t size, int top, int clear, int mir)
{
    uint8_t color = 0, prio = 0, ctrl = 0;
    int pcode = 0, ptex = 0, x, y, ox, oy;
    uint32_t pos = 0;
#define TEX() do { if (pcode & 0x20) ptex = (d[pos++] >> 1) & 0x7f; } while (0)
#define NONOP() (pos < size && d[pos] < 0xf0)
#define MX(v) (mir ? 319 - (v) : (v))

    if (clear) {
        gfx_clear(WHITE, 0, 0);
    }
    pic_stats.pics++;
    while (pos < size) {
        uint8_t op = d[pos++];
        if (op < 0xf0) {
            pic_stats.unknown++;
            continue;
        }
        pic_stats.opcodes[op - 0xf0]++;
        switch (op) {
        case 0xf0: color = d[pos++]; break;
        case 0xf1: color = 255; break;
        case 0xf2: prio = d[pos++] & 0x0f; break;
        case 0xf3: prio = 255; break;
        case 0xfb: ctrl = d[pos++] & 0x0f; break;
        case 0xfc: ctrl = 255; break;
        case 0xf9: pcode = d[pos++]; break;

        case 0xf4: /* short relative patterns */
            TEX(); abs_coords(d, &pos, &x, &y);
            draw_pattern(MX(x), y + top, color, prio, ctrl, pcode, ptex);
            while (NONOP()) {
                TEX(); rel_coords(d, &pos, &x, &y, 0);
                draw_pattern(MX(x), y + top, color, prio, ctrl, pcode, ptex);
            }
            break;
        case 0xfd: /* medium relative patterns */
            TEX(); abs_coords(d, &pos, &x, &y);
            draw_pattern(MX(x), y + top, color, prio, ctrl, pcode, ptex);
            while (NONOP()) {
                TEX(); rel_coords_med(d, &pos, &x, &y, 0);
                draw_pattern(MX(x), y + top, color, prio, ctrl, pcode, ptex);
            }
            break;
        case 0xfa: /* absolute patterns */
            while (NONOP()) {
                TEX(); abs_coords(d, &pos, &x, &y);
                draw_pattern(MX(x), y + top, color, prio, ctrl, pcode, ptex);
            }
            break;

        case 0xf5: /* medium lines */
        case 0xf6: /* long lines */
        case 0xf7: /* short lines */
            abs_coords(d, &pos, &ox, &oy);
            while (NONOP()) {
                x = ox; y = oy;
                if (op == 0xf5) rel_coords_med(d, &pos, &x, &y, 0);
                else if (op == 0xf6) abs_coords(d, &pos, &x, &y);
                else rel_coords(d, &pos, &x, &y, 0);
                gfx_draw_line(MX(ox), oy + top, MX(x), y + top, color, prio, ctrl);
                pic_stats.lines++;
                ox = x; oy = y;
            }
            break;

        case 0xf8: /* fill */
            while (NONOP()) {
                abs_coords(d, &pos, &x, &y);
                flood_fill(MX(x), y + top, color, prio, ctrl);
                pic_stats.fills++;
            }
            break;

        case 0xfe: {
            uint8_t ext = d[pos++];
            if (ext < 16) pic_stats.ext_opcodes[ext]++;
            switch (ext) {
            case 0: /* set palette entries (ignored in VGA) */
                while (NONOP()) pos++;
                break;
            case 1: { /* embedded view */
                abs_coords(d, &pos, &x, &y);
                uint16_t sz = rd16(d + pos);
                pos += 2;
                CelInfo c;
                c.width = rd16(d + pos);
                c.height = rd16(d + pos + 2);
                c.dx = (int8_t)d[pos + 4];
                c.dy = d[pos + 5];
                c.clear_key = d[pos + 6];
                c.rle = d + pos + 8;
                gfx_draw_cel_raw(&c, mir, mir ? 320 - x - c.width : x, y + top,
                                 prio == 255 ? 0xff : prio);
                pic_stats.embedded++;
                pos += sz;
                break;
            }
            case 2: /* set palette */
                gfx_palette_from_data(d + pos, 1);
                pos += 256 + 4 + 1024;
                break;
            case 3: /* priority bands, equal distance */
                gfx_set_priority_bands(rd16(d + pos), rd16(d + pos + 2));
                pos += 4;
                break;
            case 4: /* explicit priority table */
                set_priority_table(d + pos);
                pos += 14;
                break;
            default:
                pic_stats.unknown++;
                return -1;
            }
            break;
        }
        case 0xff:
            return 0;
        }
    }
    return 0;
#undef TEX
#undef NONOP
#undef MX
}

/* ------------------------------------------------------------------ fonts */
/* SCI font: u16 lowChar, u16 numChars, u16 lineHeight, u16 offsets[numChars];
 * char: u8 width, u8 height, rows of (width+7)/8 bytes, MSB first. */
int font_height(const uint8_t *font) { return rd16(font + 4); }

int font_char_width(const uint8_t *font, int ch)
{
    if (ch < 0 || ch >= rd16(font + 2))
        return 0;
    return font[rd16(font + 6 + ch * 2)];
}

int font_text_width(const uint8_t *font, const char *s)
{
    int w = 0;
    for (; *s; s++)
        w += font_char_width(font, (uint8_t)*s);
    return w;
}

void gfx_draw_text(const uint8_t *font, const char *s, int x, int y, uint8_t color)
{
    int n = rd16(font + 2);
    for (; *s; s++) {
        int ch = (uint8_t)*s;
        if (ch >= n)
            continue;
        const uint8_t *g = font + rd16(font + 6 + ch * 2);
        int w = g[0], h = g[1], bpr = (w + 7) / 8;
        const uint8_t *bits = g + 2;
        for (int j = 0; j < h; j++)
            for (int i = 0; i < w; i++)
                if (bits[j * bpr + (i >> 3)] & (0x80 >> (i & 7)))
                    put_pixel(x + i, y + j, MASK_VISUAL, color, 0, 0);
        x += w;
    }
}
