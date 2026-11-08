/* SCI kernel: graphics, ports/windows, text, controls, events, actors, motion.
 * Behaviour follows the SCI0/SCI1-early interpreter (GfxPorts, GfxPaint16,
 * GfxText16, GfxControls16, GfxAnimate, GfxCompare semantics). */
#include <stdlib.h>
#include <string.h>
#include "kernel.h"
#include "gfx.h"
#include "balloon.h"
#include "locpanel.h"
#include "resource.h"

#define ARG(i)   hp16((uint16_t)(argv + 2 * (i)))
#define SARG(i)  ((int16_t)ARG(i))

/* ------------------------------------------------------------------ rects */

typedef struct { int16_t top, left, bottom, right; } Rect;   /* bottom/right exclusive */

static int rect_empty(const Rect *r) { return r->right <= r->left || r->bottom <= r->top; }
static void rect_clip(Rect *r, const Rect *c)
{
    if (r->top < c->top) r->top = c->top;
    if (r->left < c->left) r->left = c->left;
    if (r->bottom > c->bottom) r->bottom = c->bottom;
    if (r->right > c->right) r->right = c->right;
}
static void rect_grow(Rect *r, int n) { r->top -= n; r->left -= n; r->bottom += n; r->right += n; }
static void rect_move(Rect *r, int dx, int dy) { r->top += dy; r->bottom += dy; r->left += dx; r->right += dx; }
static void rect_extend(Rect *r, const Rect *o)
{
    if (o->top < r->top) r->top = o->top;
    if (o->left < r->left) r->left = o->left;
    if (o->bottom > r->bottom) r->bottom = o->bottom;
    if (o->right > r->right) r->right = o->right;
}
static const Rect SCREEN_RECT = { 0, 0, SCREEN_H, SCREEN_W };

/* --------------------------------------------------------------- selectors */

static int S_x, S_y, S_z, S_view, S_loop, S_cel, S_priority, S_signal, S_underBits;
static int S_nsTop, S_nsLeft, S_nsBottom, S_nsRight, S_lsTop, S_lsLeft, S_lsBottom, S_lsRight;
static int S_brTop, S_brLeft, S_brBottom, S_brRight, S_illegalBits, S_xStep, S_yStep;
static int S_client, S_dx, S_dy, S_b_i1, S_b_i2, S_b_di, S_b_incr, S_b_xAxis, S_b_movCnt;
static int S_moveSpeed, S_xLast, S_yLast, S_moveDone, S_canBeHere, S_cantBeHere, S_doit;
static int S_delete, S_type, S_message, S_modifiers, S_state, S_text, S_font, S_mode;
static int S_max, S_cursor, S_palette, S_topString, S_number, S_handle, S_signal_snd;
static int S_nodePtr, S_min, S_vol, S_loop_snd, S_dataInc, S_prevSignal;

#define GS(o, s)      ((int16_t)get_sel((o), (s)))
#define SS(o, s, v)   set_sel((o), (s), (reg_t)(v))

static void init_selectors(void)
{
#define L(v, n) v = sel_id(n)
    L(S_x, "x"); L(S_y, "y"); L(S_z, "z"); L(S_view, "view"); L(S_loop, "loop"); L(S_cel, "cel");
    L(S_priority, "priority"); L(S_signal, "signal"); L(S_underBits, "underBits");
    L(S_nsTop, "nsTop"); L(S_nsLeft, "nsLeft"); L(S_nsBottom, "nsBottom"); L(S_nsRight, "nsRight");
    L(S_lsTop, "lsTop"); L(S_lsLeft, "lsLeft"); L(S_lsBottom, "lsBottom"); L(S_lsRight, "lsRight");
    L(S_brTop, "brTop"); L(S_brLeft, "brLeft"); L(S_brBottom, "brBottom"); L(S_brRight, "brRight");
    L(S_illegalBits, "illegalBits"); L(S_xStep, "xStep"); L(S_yStep, "yStep");
    L(S_client, "client"); L(S_dx, "dx"); L(S_dy, "dy"); L(S_b_i1, "b-i1"); L(S_b_i2, "b-i2");
    L(S_b_di, "b-di"); L(S_b_incr, "b-incr"); L(S_b_xAxis, "b-xAxis"); L(S_b_movCnt, "b-moveCnt");
    L(S_moveSpeed, "moveSpeed"); L(S_xLast, "xLast"); L(S_yLast, "yLast"); L(S_moveDone, "moveDone");
    L(S_canBeHere, "canBeHere"); L(S_cantBeHere, "cantBeHere"); L(S_doit, "doit");
    L(S_delete, "delete"); L(S_type, "type"); L(S_message, "message"); L(S_modifiers, "modifiers");
    L(S_state, "state"); L(S_text, "text"); L(S_font, "font"); L(S_mode, "mode"); L(S_max, "max");
    L(S_cursor, "cursor"); L(S_palette, "palette"); L(S_topString, "topString");
    L(S_number, "number"); L(S_handle, "handle"); L(S_signal_snd, "signal"); L(S_nodePtr, "nodePtr");
    L(S_min, "min"); L(S_vol, "vol"); L(S_loop_snd, "loop"); L(S_dataInc, "dataInc");
    L(S_prevSignal, "prevSignal");
#undef L
}

/* ------------------------------------------------------------------- ports */

#define MAX_PORTS 24
#define WSTYLE_TRANSPARENT 0x01
#define WSTYLE_NOFRAME     0x02
#define WSTYLE_TITLE       0x04
#define WSTYLE_TOPMOST     0x08
#define WSTYLE_USER        0x80

typedef struct {
    int      used;
    int16_t  top, left;          /* origin in screen coords */
    Rect     rect;               /* clip rect, port-relative */
    int16_t  cur_top, cur_left;  /* pen */
    int16_t  font;
    uint8_t  pen, back;
    int      greyed, pen_mode;
    /* window */
    int      is_window;
    Rect     dims, restore;
    int      style, save_mask, drawn;
    uint16_t saved1, saved2;
    char     title[64];
} Port;

static Port ports[MAX_PORTS];
static int cur_port;
static int wmgr_port = 1, pic_port = 2;
static int window_order[MAX_PORTS], nwindows;   /* back to front */
static int pic_not_valid;
static int default_font;

static Port *P(void) { return &ports[cur_port]; }

static void port_open(Port *p)
{
    p->font = (int16_t)default_font;
    p->pen = 0;
    p->back = 255;
    p->greyed = 0;
    p->pen_mode = 0;
    p->cur_top = p->cur_left = 0;
    p->top = p->left = 0;
    p->rect = SCREEN_RECT;
}

static int set_port(int id)
{
    int old = cur_port;
    if (id > 0 && id < MAX_PORTS && ports[id].used)
        cur_port = id;
    return old;
}

static void offset_rect(Rect *r) { rect_move(r, P()->left, P()->top); }

/* ---------------------------------------------------------- bits handles */

#define MAX_BITS 96
typedef struct { int used; Rect r; int mask; uint8_t *data; } SavedBits;
static SavedBits bits[MAX_BITS];

/* rect in screen coords */
static uint16_t bits_save_screen(Rect r, int mask)
{
    rect_clip(&r, &SCREEN_RECT);
    if (rect_empty(&r)) return 0;
    int w = r.right - r.left, h = r.bottom - r.top, planes = 0;
    for (int m = 1; m <= 4; m <<= 1) if (mask & m) planes++;
    for (int i = 1; i < MAX_BITS; i++) {
        if (bits[i].used) continue;
        uint8_t *d = malloc((size_t)w * h * planes);
        if (!d) return 0;
        uint8_t *o = d;
        for (int m = 1; m <= 4; m <<= 1) {
            if (!(mask & m)) continue;
            uint8_t *src = m == 1 ? gfx.visual : m == 2 ? gfx.priority : gfx.control;
            for (int y = r.top; y < r.bottom; y++) {
                memcpy(o, src + y * SCREEN_W + r.left, w);
                o += w;
            }
        }
        bits[i] = (SavedBits){ 1, r, mask, d };
        return (uint16_t)(0xb000 | i);
    }
    vm_log("out of saved-bits handles\n");
    return 0;
}

/* rect in port coords, clipped to port */
static uint16_t bits_save(Rect r, int mask)
{
    rect_clip(&r, &P()->rect);
    if (rect_empty(&r)) return 0;
    offset_rect(&r);
    return bits_save_screen(r, mask);
}

static SavedBits *bits_get(uint16_t h)
{
    if ((h & 0xff00) != 0xb000) return NULL;
    int i = h & 0xff;
    return (i > 0 && i < MAX_BITS && bits[i].used) ? &bits[i] : NULL;
}

static void bits_free(uint16_t h)
{
    SavedBits *b = bits_get(h);
    if (!b) return;
    free(b->data);
    b->used = 0;
}

static void bits_restore(uint16_t h)
{
    SavedBits *b = bits_get(h);
    if (!b) return;
    int w = b->r.right - b->r.left;
    uint8_t *o = b->data;
    for (int m = 1; m <= 4; m <<= 1) {
        if (!(b->mask & m)) continue;
        uint8_t *dst = m == 1 ? gfx.visual : m == 2 ? gfx.priority : gfx.control;
        for (int y = b->r.top; y < b->r.bottom; y++) {
            memcpy(dst + y * SCREEN_W + b->r.left, o, w);
            o += w;
        }
    }
    bits_free(h);
}

/* copy screen-rect of visual to display */
static void show_screen_rect(Rect r)
{
    rect_clip(&r, &SCREEN_RECT);
    if (rect_empty(&r)) return;
    for (int y = r.top; y < r.bottom; y++)
        memcpy(gfx.display + y * SCREEN_W + r.left, gfx.visual + y * SCREEN_W + r.left, r.right - r.left);
    gfx.display_dirty = 1;
}

static void bits_show(Rect r)   /* port coords */
{
    rect_clip(&r, &P()->rect);
    offset_rect(&r);
    show_screen_rect(r);
}

static void show_all(void)
{
    memcpy(gfx.display, gfx.visual, sizeof gfx.display);
    gfx.display_dirty = 1;
}

void kgfx_present(void)
{
    static uint32_t balloon_sig;
    uint32_t bs = balloon_signature(plat_ticks());
    if (bs != balloon_sig) { balloon_sig = bs; gfx.display_dirty = 1; }
    if (gfx.display_dirty || gfx.palette_dirty) {
        plat_present();
        gfx.display_dirty = 0;
    }
}

/* ----------------------------------------------------------------- paint */

static void fill_rect(Rect r, int mask, uint8_t color, uint8_t prio, uint8_t ctrl)
{
    rect_clip(&r, &P()->rect);
    if (rect_empty(&r)) return;
    offset_rect(&r);
    rect_clip(&r, &SCREEN_RECT);
    int invert = P()->pen_mode == 2;
    for (int y = r.top; y < r.bottom; y++) {
        int o = y * SCREEN_W;
        for (int x = r.left; x < r.right; x++) {
            if (invert) {
                uint8_t v = gfx.visual[o + x];
                if (v == color) gfx.visual[o + x] = prio;
                else if (v == prio) gfx.visual[o + x] = color;
                continue;
            }
            if (mask & MASK_VISUAL) gfx.visual[o + x] = color;
            if (mask & MASK_PRIORITY) gfx.priority[o + x] = prio;
            if (mask & MASK_CONTROL) gfx.control[o + x] = ctrl;
        }
    }
}

static void erase_rect(Rect r) { fill_rect(r, MASK_VISUAL, P()->back, 0, 0); }

static void frame_rect(Rect r)
{
    Rect t;
    uint8_t c = P()->pen;
    t = r; t.bottom = (int16_t)(t.top + 1); fill_rect(t, MASK_VISUAL, c, 0, 0);
    t = r; t.top = (int16_t)(t.bottom - 1); fill_rect(t, MASK_VISUAL, c, 0, 0);
    t = r; t.right = (int16_t)(t.left + 1); fill_rect(t, MASK_VISUAL, c, 0, 0);
    t = r; t.left = (int16_t)(t.right - 1); fill_rect(t, MASK_VISUAL, c, 0, 0);
}

static void invert_rect(Rect r)
{
    int old = P()->pen_mode;
    P()->pen_mode = 2;
    fill_rect(r, MASK_VISUAL, P()->pen, P()->back, 0);
    P()->pen_mode = old;
}

/* --------------------------------------------------------------- views */

static const uint8_t *view_get(int num, uint16_t *size)
{
    const uint8_t *v = res_load(RES_VIEW, num, size);
    if (v) res_unlock(RES_VIEW, num);
    return v;
}

static int cel_rect(int view, int loop, int cel, int x, int y, int z, Rect *out)
{
    uint16_t sz;
    const uint8_t *v = view_get(view, &sz);
    CelInfo c;
    int mir = 0;
    if (!v || !view_get_cel(v, loop, cel, &c, &mir)) {
        *out = (Rect){ (int16_t)y, (int16_t)x, (int16_t)y, (int16_t)x };
        return 0;
    }
    int dx = mir ? -c.dx : c.dx;
    out->left = (int16_t)(x + dx - (c.width >> 1));
    out->right = (int16_t)(out->left + c.width);
    out->bottom = (int16_t)(y + c.dy - z + 1);
    out->top = (int16_t)(out->bottom - c.height);
    return 1;
}

static uint8_t celbuf[SCREEN_W * SCREEN_H];

/* draw cel with top-left at r.left/r.top (port coords), clipped to port */
static void draw_cel(int view, int loop, int cel, Rect r, int priority)
{
    uint16_t sz;
    const uint8_t *v = view_get(view, &sz);
    CelInfo c;
    int mir = 0;
    if (!v || !view_get_cel(v, loop, cel, &c, &mir)) return;
    const uint8_t *map = view_apply_palette(v, sz);
    if ((uint32_t)c.width * c.height > sizeof celbuf) return;
    cel_decode(&c, celbuf);
    Rect clip = { r.top, r.left, (int16_t)(r.top + c.height), (int16_t)(r.left + c.width) };
    rect_clip(&clip, &P()->rect);
    if (rect_empty(&clip)) return;
    balloon_cel_drawn(view, r.left + P()->left, r.top + P()->top, c.width, c.height);
    uint8_t pr = (uint8_t)priority;
    int write_pri = pr <= 15;
    for (int y = clip.top; y < clip.bottom; y++) {
        int sy = y + P()->top;
        if ((unsigned)sy >= SCREEN_H) continue;
        int row = (y - r.top) * c.width;
        for (int x = clip.left; x < clip.right; x++) {
            int sx = x + P()->left;
            if ((unsigned)sx >= SCREEN_W) continue;
            int cx = x - r.left;
            uint8_t col = celbuf[row + (mir ? c.width - 1 - cx : cx)];
            if (col == c.clear_key) continue;
            int o = sy * SCREEN_W + sx;
            if (pr < gfx.priority[o]) continue;
            gfx.visual[o] = map ? map[col] : col;
            if (write_pri) gfx.priority[o] = pr;
        }
    }
}

static int coord_to_priority(int y)
{
    if (y < 0) y = 0;
    if (y > 199) y = 199;
    return gfx.priority_bands[y];
}

static int priority_to_coord(int p)
{
    for (int y = 0; y < 200; y++)
        if (gfx.priority_bands[y] == p) return y;
    return 199;
}

/* ---------------------------------------------------------------- fonts */

static const uint8_t *font_get(int num)
{
    uint16_t sz;
    const uint8_t *f = res_load(RES_FONT, num, &sz);
    if (!f && num != default_font) f = res_load(RES_FONT, default_font, &sz);
    return f;
}

static int char_w(const uint8_t *f, uint8_t ch)
{
    if (ch == '\n' || ch == '\r') return 0;
    return font_char_width(f, ch);
}

/* returns char count for one line; advances *pp */
static int get_longest(const char **pp, int maxw, const uint8_t *f)
{
    const char *p = *pp, *last_space = NULL;
    int count = 0, last_space_count = 0, w = 0;
    for (;;) {
        uint8_t ch = (uint8_t)*p;
        if (ch == '\r') {
            if (p[1] == '\n') { count++; p++; }
            count++; p++;
            *pp = p;
            return count;
        }
        if (ch == '\n') { count++; p++; *pp = p; return count; }
        if (ch == 0) { *pp = p; return count; }
        if (ch == ' ') { last_space_count = count; last_space = p + 1; }
        w += char_w(f, ch);
        if (w > maxw) break;
        count++;
        p++;
    }
    if (last_space_count) { *pp = last_space; return last_space_count; }
    *pp = p;
    return count;
}

static int text_width_n(const char *s, int n, const uint8_t *f)
{
    int w = 0;
    for (int i = 0; i < n && s[i]; i++) w += char_w(f, (uint8_t)s[i]);
    return w;
}

static void text_size(Rect *r, const char *text, int font, int maxw)
{
    const uint8_t *f = font_get(font);
    r->top = r->left = 0;
    if (!f) { r->bottom = r->right = 0; return; }
    int fh = font_height(f);
    if (maxw < 0) {
        r->bottom = (int16_t)fh;
        r->right = (int16_t)text_width_n(text, 10000, f);
        return;
    }
    int width = maxw ? maxw : 192, maxt = 0, total = 0;
    const char *p = text, *line = text;
    while (*p) {
        int n = get_longest(&p, width, f);
        if (n == 0) break;
        int tw = text_width_n(line, n, f);
        if (tw > maxt) maxt = tw;
        total += fh;
        line = p;
    }
    r->bottom = (int16_t)total;
    r->right = (int16_t)(maxw ? maxw : (maxt < width ? maxt : width));
}

/* draw n chars at the pen position (port coords), advancing the pen */
static void draw_chars(const char *s, int n, const uint8_t *f)
{
    Port *p = P();
    int numchars = f[2] | f[3] << 8;
    for (int i = 0; i < n && s[i]; i++) {
        uint8_t ch = (uint8_t)s[i];
        if (ch == '\n' || ch == '\r' || ch >= numchars) continue;
        const uint8_t *g = f + (f[6 + ch * 2] | f[7 + ch * 2] << 8);
        int w = g[0], h = g[1], bpr = (w + 7) / 8;
        for (int y = 0; y < h; y++) {
            int py = p->cur_top + y;
            if (py < p->rect.top || py >= p->rect.bottom) continue;
            int sy = py + p->top;
            if ((unsigned)sy >= SCREEN_H) continue;
            for (int x = 0; x < w; x++) {
                if (!(g[2 + y * bpr + (x >> 3)] & (0x80 >> (x & 7)))) continue;
                int px = p->cur_left + x;
                if (px < p->rect.left || px >= p->rect.right) continue;
                if (p->greyed && ((px + py) & 1)) continue;
                int sx = px + p->left;
                if ((unsigned)sx >= SCREEN_W) continue;
                gfx.visual[sy * SCREEN_W + sx] = p->pen;
            }
        }
        p->cur_left = (int16_t)(p->cur_left + w);
    }
}

static void text_box(const char *text, Rect r, int align, int font)
{
    const uint8_t *f = font_get(font >= 0 ? font : P()->font);
    if (!f) return;
    int fh = font_height(f), hline = 0, width = r.right - r.left;
    const char *p = text, *line = text;
    while (*p) {
        int n = get_longest(&p, width, f);
        if (n == 0) break;
        int tw = text_width_n(line, n, f), off = 0;
        if (align == -1) off = width - tw;
        else if (align == 1) off = (width - tw) / 2;
        P()->cur_left = (int16_t)(r.left + off);
        P()->cur_top = (int16_t)(r.top + hline);
        draw_chars(line, n, f);
        hline += fh;
        line = p;
    }
}

/* ---------------------------------------------------------------- windows */

static void window_list_remove(int id)
{
    for (int i = 0; i < nwindows; i++)
        if (window_order[i] == id) {
            memmove(&window_order[i], &window_order[i + 1], (nwindows - i - 1) * sizeof(int));
            nwindows--;
            return;
        }
}

static void draw_window(Port *w)
{
    if (w->drawn) return;
    int old = set_port(wmgr_port);
    P()->pen = 0;
    if (!(w->style & WSTYLE_TRANSPARENT)) {
        w->saved1 = bits_save_screen(w->restore, MASK_VISUAL);
        if (w->save_mask & MASK_PRIORITY) {
            w->saved2 = bits_save_screen(w->restore, MASK_PRIORITY);
            if (!(w->style & WSTYLE_USER)) {
                Rect r = w->restore;
                fill_rect(r, MASK_PRIORITY, 0, 15, 0);
            }
        }
    }
    if (!(w->style & WSTYLE_USER)) {
        Rect r = w->dims;
        if (!(w->style & WSTYLE_NOFRAME)) {
            r.top++; r.left++;
            frame_rect(r);
            rect_move(&r, -1, -1);
            frame_rect(r);
            if (w->style & WSTYLE_TITLE) {
                rect_grow(&r, -1);
                fill_rect(r, MASK_VISUAL, 0, 0, 0);   /* black titlebar (SCI01+) */
                Rect tr = r;
                tr.bottom = (int16_t)(tr.top + 9);
                if (w->title[0]) {
                    uint8_t oldpen = P()->pen;
                    P()->pen = 255;
                    text_box(w->title, tr, 1, 0);
                    P()->pen = oldpen;
                }
                rect_grow(&r, 1);
                r.bottom = (int16_t)(w->dims.bottom - 1);
                r.top = (int16_t)(r.top + 9);
            }
            rect_grow(&r, -1);
        }
        if (!(w->style & WSTYLE_TRANSPARENT))
            fill_rect(r, MASK_VISUAL, w->back, 0, 0);
        show_screen_rect(w->dims);
    }
    w->drawn = 1;
    set_port(old);
}

static int new_window(Rect dims, const Rect *restore, const char *title, int style, int priority,
                      uint8_t pen, uint8_t back)
{
    int id = 0;
    for (int i = 3; i < MAX_PORTS; i++) if (!ports[i].used) { id = i; break; }
    if (!id) { vm_log("out of windows\n"); return 0; }
    Port *w = &ports[id];
    memset(w, 0, sizeof *w);
    w->used = 1;
    w->is_window = 1;
    port_open(w);
    if (style & WSTYLE_TOPMOST) {
        memmove(&window_order[1], &window_order[0], nwindows * sizeof(int));
        window_order[0] = id;
    } else {
        window_order[nwindows] = id;
    }
    nwindows++;
    w->rect = dims;
    w->style = style;
    w->save_mask = (style & WSTYLE_TRANSPARENT) ? 0 :
                   (priority == -1 ? MASK_VISUAL : MASK_VISUAL | MASK_PRIORITY);
    if (title && (style & WSTYLE_TITLE)) {
        strncpy(w->title, title, sizeof w->title - 1);
    }
    Rect r = w->rect;
    if (!(style & WSTYLE_USER) && !(style & WSTYLE_NOFRAME)) {
        rect_grow(&r, 1);
        if (style & WSTYLE_TITLE) { r.top -= 10; r.bottom++; }
    }
    w->dims = r;
    Rect wm = ports[wmgr_port].rect;
    int16_t ot = w->dims.top, ol = w->dims.left;
    if (wm.top > w->dims.top) rect_move(&w->dims, 0, wm.top - w->dims.top);
    if (wm.bottom < w->dims.bottom) rect_move(&w->dims, 0, wm.bottom - w->dims.bottom);
    if (wm.right < w->dims.right) rect_move(&w->dims, wm.right - w->dims.right, 0);
    if (wm.left > w->dims.left) rect_move(&w->dims, wm.left - w->dims.left, 0);
    rect_move(&w->rect, w->dims.left - ol, w->dims.top - ot);
    w->restore = w->dims;
    if (restore && !rect_empty(restore)) {   /* never leave the frame behind */
        Rect u = *restore;
        if (u.top > w->dims.top) u.top = w->dims.top;
        if (u.left > w->dims.left) u.left = w->dims.left;
        if (u.bottom < w->dims.bottom) u.bottom = w->dims.bottom;
        if (u.right < w->dims.right) u.right = w->dims.right;
        w->restore = u;
    }
    w->pen = pen;
    w->back = back;
    draw_window(w);
    set_port(id);
    w->left = w->rect.left;
    w->top = (int16_t)(w->rect.top + ports[wmgr_port].top);
    rect_move(&w->rect, -w->rect.left, -w->rect.top);
    locpanel_window_open(id, w->dims.left, w->dims.top, w->dims.right, w->dims.bottom);
    return id;
}

static void redraw_cast_box(Rect r);

static void dispose_window(int id, int reanimate)
{
    if (id < 3 || id >= MAX_PORTS || !ports[id].used) return;
    Port *w = &ports[id];
    set_port(wmgr_port);
    bits_restore(w->saved1);
    bits_restore(w->saved2);
    if (!reanimate) show_screen_rect(w->restore);
    else redraw_cast_box(w->restore);
    window_list_remove(id);
    w->used = 0;
    locpanel_window_close(id);
    set_port(nwindows ? window_order[nwindows - 1] : wmgr_port);
}

static void ports_init(void)
{
    memset(ports, 0, sizeof ports);
    locpanel_reset();
    for (int i = 1; i < MAX_BITS; i++) if (bits[i].used) bits_free((uint16_t)(0xb000 | i));
    nwindows = 0;
    /* window manager port: whole screen (Jones runs with -w 0 0 200 320) */
    Port *wm = &ports[wmgr_port];
    wm->used = 1;
    port_open(wm);
    wm->rect = (Rect){ 0, 0, SCREEN_H, SCREEN_W };
    window_order[nwindows++] = wmgr_port;
    /* picture window */
    Port *pw = &ports[pic_port];
    pw->used = 1;
    port_open(pw);
    pw->is_window = 1;
    pw->style = WSTYLE_TRANSPARENT | WSTYLE_NOFRAME;
    pw->rect = (Rect){ 0, 0, SCREEN_H, SCREEN_W };
    pw->dims = pw->restore = pw->rect;
    pw->drawn = 1;
    window_order[nwindows++] = pic_port;
    cur_port = pic_port;
    gfx_set_priority_bands(42, 200);
}

/* ---------------------------------------------------------------- cursor */

static int cursor_visible = 1, cursor_id = 999;

static void set_cursor_shape(int id)
{
    if (id == -1) {
        cursor_visible = 0;
        plat_set_cursor(NULL, 0, 0, 0, 0);
        return;
    }
    cursor_visible = 1;
    cursor_id = id;
    uint16_t sz;
    const uint8_t *c = res_load(RES_CURSOR, id, &sz);
    plat_set_cursor(c, 0, 0, 0, 1);
}

/* ---------------------------------------------------------------- picture */

static void draw_picture(int num, int mirrored, int add_to)
{
    uint16_t sz;
    const uint8_t *d = res_load(RES_PIC, num, &sz);
    if (!d) { vm_log("pic %d missing\n", num); return; }
    int old = set_port(pic_port);
    if (!add_to) {
        gfx_clear(255, 0, 0);
    }
    gfx_draw_pic(d, sz, P()->top, 0, mirrored);
    res_unlock(RES_PIC, num);
    set_port(old);
}

/* --------------------------------------------------------------- animate */

#define SIG_STOPUPDATE    0x0001
#define SIG_VIEWUPDATED   0x0002
#define SIG_NOUPDATE      0x0004
#define SIG_HIDDEN        0x0008
#define SIG_FIXEDPRI      0x0010
#define SIG_ALWAYSUPDATE  0x0020
#define SIG_FORCEUPDATE   0x0040
#define SIG_REMOVEVIEW    0x0080
#define SIG_FROZEN        0x0100
#define SIG_HITOBSTACLE   0x0400
#define SIG_DOESNTTURN    0x0800
#define SIG_IGNOREACTOR   0x4000
#define SIG_DISPOSEME     0x8000

typedef struct {
    reg_t obj;
    int order, view, loop, cel, x, y, z, priority;
    uint16_t signal;
    Rect cel_rect;
    int show_bits;
} AnimEntry;

#define MAX_CAST 64
static AnimEntry cast[MAX_CAST];
static int ncast;

static int cast_cmp(const void *a, const void *b)
{
    const AnimEntry *x = a, *y = b;
    if (x->y != y->y) return x->y - y->y;
    if (x->z != y->z) return x->z - y->z;
    return x->order - y->order;
}

static void make_sorted_list(reg_t list)
{
    ncast = 0;
    for (reg_t n = list ? hp16(list) : 0; n && ncast < MAX_CAST; n = hp16((uint16_t)(n + 2))) {
        reg_t o = hp16((uint16_t)(n + 6));
        AnimEntry *e = &cast[ncast];
        e->obj = o;
        e->order = ncast;
        e->view = GS(o, S_view);
        e->loop = GS(o, S_loop);
        e->cel = GS(o, S_cel);
        e->x = GS(o, S_x);
        e->y = GS(o, S_y);
        e->z = S_z >= 0 ? GS(o, S_z) : 0;
        e->priority = GS(o, S_priority);
        e->signal = (uint16_t)GS(o, S_signal);
        e->show_bits = 0;
        ncast++;
    }
    qsort(cast, ncast, sizeof *cast, cast_cmp);
}

static void set_ns_rect(reg_t o, const Rect *r)
{
    if (obj_find_var(o, S_nsTop) < 0) return;
    SS(o, S_nsLeft, r->left); SS(o, S_nsTop, r->top);
    SS(o, S_nsRight, r->right); SS(o, S_nsBottom, r->bottom);
}

static void anim_fill(int *old_pnv)
{
    for (int i = 0; i < ncast; i++) {
        AnimEntry *e = &cast[i];
        uint16_t sz;
        const uint8_t *v = view_get(e->view, &sz);
        if (v) {
            int loops = view_loop_count(v);
            if (e->loop < 0 || e->loop >= loops) { e->loop = 0; SS(e->obj, S_loop, 0); }
            if ((unsigned)e->cel >= (unsigned)view_cel_count(v, e->loop)) { e->cel = 0; SS(e->obj, S_cel, 0); }
        }
        cel_rect(e->view, e->loop, e->cel, e->x, e->y, e->z, &e->cel_rect);
        set_ns_rect(e->obj, &e->cel_rect);
        if (!(e->signal & SIG_FIXEDPRI)) {
            e->priority = coord_to_priority(e->y);
            SS(e->obj, S_priority, e->priority);
        }
        if (e->signal & SIG_NOUPDATE) {
            if ((e->signal & (SIG_FORCEUPDATE | SIG_VIEWUPDATED)) ||
                ((e->signal & SIG_HIDDEN) && !(e->signal & SIG_REMOVEVIEW)) ||
                (!(e->signal & SIG_HIDDEN) && (e->signal & SIG_REMOVEVIEW)) ||
                (e->signal & SIG_ALWAYSUPDATE))
                (*old_pnv)++;
            e->signal &= ~SIG_STOPUPDATE;
        } else {
            if ((e->signal & SIG_STOPUPDATE) || (e->signal & SIG_ALWAYSUPDATE))
                (*old_pnv)++;
            e->signal &= ~SIG_FORCEUPDATE;
        }
    }
}

static void control_box(AnimEntry *e)
{
    if (e->signal & SIG_IGNOREACTOR) return;
    Rect r = e->cel_rect;
    int t = priority_to_coord(e->priority) - 1;
    if (t < r.top) t = r.top;
    if (t > r.bottom - 1) t = r.bottom - 1;
    r.top = (int16_t)t;
    fill_rect(r, MASK_CONTROL, 0, 0, 15);
}

static void anim_update(void)
{
    for (int i = ncast - 1; i >= 0; i--) {
        AnimEntry *e = &cast[i];
        if (e->signal & SIG_NOUPDATE) {
            if (!(e->signal & SIG_REMOVEVIEW)) {
                uint16_t h = (uint16_t)GS(e->obj, S_underBits);
                if (pic_not_valid != 1) { bits_restore(h); e->show_bits = 1; }
                else bits_free(h);
                SS(e->obj, S_underBits, 0);
            }
            e->signal &= ~SIG_FORCEUPDATE;
            if (e->signal & SIG_VIEWUPDATED) e->signal &= ~(SIG_VIEWUPDATED | SIG_NOUPDATE);
        } else if (e->signal & SIG_STOPUPDATE) {
            e->signal &= ~SIG_STOPUPDATE;
            e->signal |= SIG_NOUPDATE;
        }
    }
    for (int i = 0; i < ncast; i++) {
        AnimEntry *e = &cast[i];
        if (e->signal & SIG_ALWAYSUPDATE) {
            draw_cel(e->view, e->loop, e->cel, e->cel_rect, e->priority);
            e->show_bits = 1;
            e->signal &= ~(SIG_STOPUPDATE | SIG_VIEWUPDATED | SIG_FORCEUPDATE);
            control_box(e);
        }
    }
    for (int i = 0; i < ncast; i++) {
        AnimEntry *e = &cast[i];
        if (e->signal & SIG_NOUPDATE) {
            if (e->signal & SIG_HIDDEN) {
                e->signal |= SIG_REMOVEVIEW;
            } else {
                e->signal &= ~SIG_REMOVEVIEW;
                uint16_t h = bits_save(e->cel_rect, (e->signal & SIG_IGNOREACTOR) ?
                                       (MASK_VISUAL | MASK_PRIORITY) : 7);
                SS(e->obj, S_underBits, h);
            }
        }
    }
    for (int i = 0; i < ncast; i++) {
        AnimEntry *e = &cast[i];
        if ((e->signal & SIG_NOUPDATE) && !(e->signal & SIG_HIDDEN)) {
            draw_cel(e->view, e->loop, e->cel, e->cel_rect, e->priority);
            e->show_bits = 1;
            control_box(e);
        }
    }
}

static void anim_draw_cels(void)
{
    for (int i = 0; i < ncast; i++) {
        AnimEntry *e = &cast[i];
        if (!(e->signal & (SIG_NOUPDATE | SIG_HIDDEN | SIG_ALWAYSUPDATE))) {
            uint16_t h = bits_save(e->cel_rect, 7);
            SS(e->obj, S_underBits, h);
            draw_cel(e->view, e->loop, e->cel, e->cel_rect, e->priority);
            e->show_bits = 1;
            if (e->signal & SIG_REMOVEVIEW) e->signal &= ~SIG_REMOVEVIEW;
        }
    }
}

static void anim_update_screen(int old_pnv)
{
    for (int i = 0; i < ncast; i++) {
        AnimEntry *e = &cast[i];
        if (e->show_bits || !(e->signal & (SIG_REMOVEVIEW | SIG_NOUPDATE)) ||
            (!(e->signal & SIG_REMOVEVIEW) && (e->signal & SIG_NOUPDATE) && old_pnv)) {
            Rect ls = { GS(e->obj, S_lsTop), GS(e->obj, S_lsLeft), GS(e->obj, S_lsBottom), GS(e->obj, S_lsRight) };
            Rect w = ls;
            rect_clip(&w, &e->cel_rect);
            if (!rect_empty(&w)) {
                w = ls;
                rect_extend(&w, &e->cel_rect);
            } else {
                bits_show(ls);
                w = e->cel_rect;
            }
            SS(e->obj, S_lsLeft, e->cel_rect.left); SS(e->obj, S_lsTop, e->cel_rect.top);
            SS(e->obj, S_lsRight, e->cel_rect.right); SS(e->obj, S_lsBottom, e->cel_rect.bottom);
            bits_show(w);
            if (e->signal & SIG_HIDDEN) e->signal |= SIG_REMOVEVIEW;
        }
    }
}

static void anim_restore_and_delete(int argc, uint16_t argv)
{
    for (int i = 0; i < ncast; i++)
        SS(cast[i].obj, S_signal, cast[i].signal);
    for (int i = ncast - 1; i >= 0; i--) {
        AnimEntry *e = &cast[i];
        e->signal = (uint16_t)GS(e->obj, S_signal);
        if (!(e->signal & (SIG_NOUPDATE | SIG_REMOVEVIEW))) {
            bits_restore((uint16_t)GS(e->obj, S_underBits));
            SS(e->obj, S_underBits, 0);
        }
        if (e->signal & SIG_DISPOSEME) {
            reg_t args[8];
            int n = argc > 8 ? 8 : argc;
            for (int k = 0; k < n; k++) args[k] = ARG(k);
            invoke(e->obj, S_delete, n, args);
        }
    }
}

static void anim_show_pic(void)
{
    show_all();
    pic_not_valid = 0;
}

static reg_t last_cast_list;

static void kernel_animate(reg_t list, int cycle, int argc, uint16_t argv)
{
    int old_pnv = pic_not_valid;
    if (!list) {
        if (pic_not_valid) anim_show_pic();
        kgfx_present();
        return;
    }
    last_cast_list = list;
    if (cycle) {
        for (reg_t n = hp16(list); n;) {
            reg_t o = hp16((uint16_t)(n + 6));
            if (vm_global(84)) return;   /* fastCast */
            uint16_t sig = (uint16_t)GS(o, S_signal);
            if (!(sig & SIG_FROZEN)) {
                reg_t args[8];
                int k, na = argc - 2 > 8 ? 8 : argc - 2;
                for (k = 0; k < na; k++) args[k] = ARG(2 + k);
                invoke(o, S_doit, na > 0 ? na : 0, args);
                if (vm.restarting || vm.quit) return;
                /* node may have been deleted by doit */
                if (hp16((uint16_t)(n + 6)) != o) break;
            }
            n = hp16((uint16_t)(n + 2));
        }
    }
    int old = set_port(pic_port);
    make_sorted_list(list);
    anim_fill(&old_pnv);
    if (old_pnv) anim_update();
    anim_draw_cels();
    if (old_pnv) anim_show_pic();
    anim_update_screen(old_pnv);
    anim_restore_and_delete(0, argv);
    set_port(old);
    kgfx_present();
}

static void redraw_cast_box(Rect r)
{
    /* re-animate: redraw the last cast within a rect (screen coords) */
    if (last_cast_list) {
        int old = set_port(pic_port);
        make_sorted_list(last_cast_list);
        for (int i = 0; i < ncast; i++) {
            AnimEntry *e = &cast[i];
            if (e->signal & (SIG_HIDDEN | SIG_REMOVEVIEW)) continue;
            cel_rect(e->view, e->loop, e->cel, e->x, e->y, e->z, &e->cel_rect);
            draw_cel(e->view, e->loop, e->cel, e->cel_rect, e->priority);
        }
        set_port(old);
    }
    show_screen_rect(r);
}

static void add_to_pic_list(reg_t list)
{
    int old = set_port(pic_port);
    make_sorted_list(list);
    pic_not_valid = 1;
    for (int i = 0; i < ncast; i++) {
        AnimEntry *e = &cast[i];
        if (e->priority == -1) e->priority = coord_to_priority(e->y);
        cel_rect(e->view, e->loop, e->cel, e->x, e->y, e->z, &e->cel_rect);
        draw_cel(e->view, e->loop, e->cel, e->cel_rect, e->priority);
        control_box(e);
    }
    set_port(old);
}

/* ---------------------------------------------------------------- controls */

#define CTL_BUTTON 1
#define CTL_TEXT   2
#define CTL_EDIT   3
#define CTL_ICON   4
#define CTL_LIST   6
#define CTL_LIST2  7
#define STYLE_ENABLED  0x0001
#define STYLE_SELECTED 0x0008

static Rect ns_rect(reg_t o)
{
    Rect r = { GS(o, S_nsTop), GS(o, S_nsLeft), GS(o, S_nsBottom), GS(o, S_nsRight) };
    return r;
}

static void draw_control(reg_t o, int hilite)
{
    int type = GS(o, S_type), style = GS(o, S_state), font = GS(o, S_font);
    reg_t tref = get_sel(o, S_text);
    const char *text = tref ? vm_string(tref, 0, NULL) : "";
    Rect r = ns_rect(o);
    if (getenv("GRAPHLOG")) vm_log("DrawControl %s type %d state %x rect %d,%d,%d,%d view %d loop %d cel %d\n", obj_name(o), type, style,
        r.top, r.left, r.bottom, r.right, GS(o, S_view), GS(o, S_loop), GS(o, S_cel));
    locpanel_control(cur_port, obj_name(o), type, style, GS(o, S_view),
                     r.left + P()->left, r.top + P()->top, r.right + P()->left, r.bottom + P()->top);
    switch (type) {
    case CTL_BUTTON:
        if (!hilite) {
            rect_grow(&r, 1);
            erase_rect(r);
            frame_rect(r);
            rect_grow(&r, -2);
            P()->greyed = !(style & STYLE_ENABLED);
            text_box(text, r, 1, font);
            P()->greyed = 0;
            rect_grow(&r, 1);
            if (style & STYLE_SELECTED) frame_rect(r);
            if (!pic_not_valid) { rect_grow(&r, 1); bits_show(r); }
        } else {
            invert_rect(r);
            bits_show(r);
        }
        break;
    case CTL_TEXT:
        if (!hilite) {
            int align = GS(o, S_mode);
            rect_grow(&r, 1);
            erase_rect(r);
            rect_grow(&r, -1);
            text_box(text, r, align, font);
            if (style & STYLE_SELECTED) frame_rect(r);
            if (!pic_not_valid) bits_show(r);
        } else {
            invert_rect(r);
            bits_show(r);
        }
        break;
    case CTL_EDIT: {
        if (!hilite) {
            rect_grow(&r, 1);
            erase_rect(r);
            frame_rect(r);
            rect_grow(&r, -1);
            text_box(text, r, 0, font);
            /* cursor */
            int cur = GS(o, S_cursor);
            const uint8_t *f = font_get(font);
            if (f && (style & STYLE_SELECTED)) {
                int cx = r.left + text_width_n(text, cur, f);
                Rect cr = { r.top, (int16_t)cx, (int16_t)(r.top + font_height(f)), (int16_t)(cx + 1) };
                invert_rect(cr);
            }
            if (!pic_not_valid) { rect_grow(&r, 1); bits_show(r); }
        } else {
            invert_rect(r);
            bits_show(r);
        }
        break;
    }
    case CTL_ICON: {
        if (!hilite) {
            int view = GS(o, S_view);
            int l = GS(o, S_loop) & 0xff, c = GS(o, S_cel) & 0xff;
            int loop = (l & 0x80) ? l - 256 : l, cel = (c & 0x80) ? c - 256 : c;
            int pri = obj_find_var(o, S_priority) >= 0 ? GS(o, S_priority) : -1;
            Rect cr = r;
            draw_cel(view, loop, cel, cr, pri == -1 ? 255 : pri);
            if (style & 0x20) frame_rect(r);
            if (!pic_not_valid) bits_show(r);
        } else {
            invert_rect(r);
            bits_show(r);
        }
        break;
    }
    case CTL_LIST:
    case CTL_LIST2: {
        /* selector list: text points at an array of fixed-width strings */
        int maxw = GS(o, S_max) + 1, top = GS(o, S_topString), cur = GS(o, S_cursor);
        if (!hilite) {
            rect_grow(&r, 1);
            erase_rect(r);
            frame_rect(r);
            rect_grow(&r, -1);
            const uint8_t *f = font_get(font);
            int fh = f ? font_height(f) : 8;
            int y = r.top;
            for (int i = top; tref && y + fh <= r.bottom; i++) {
                const char *s = hstr((uint16_t)(tref + i * maxw));
                if (!*s) break;
                Rect lr = { (int16_t)y, r.left, (int16_t)(y + fh), r.right };
                text_box(s, lr, 0, font);
                if (i == cur) invert_rect(lr);
                y += fh;
            }
            if (!pic_not_valid) { rect_grow(&r, 1); bits_show(r); }
        } else {
            invert_rect(r);
            bits_show(r);
        }
        break;
    }
    default:
        break;
    }
}

/* ---------------------------------------------------------------- display */

static reg_t kernel_display(int argc, uint16_t argv)
{
    int used = 0;
    const char *text = vm_string(ARG(0), argc > 1 ? ARG(1) : 0, &used);
    static char buf[1024];
    strncpy(buf, text, sizeof buf - 1);
    buf[sizeof buf - 1] = 0;
    if (getenv("GRAPHLOG")) {
        vm_log("Display '%.20s' port %d:", text, cur_port);
        for (int k = used ? 2 : 1; k < argc; k++) vm_log(" %d", SARG(k));
        vm_log("\n");
    }
    int i = used ? 2 : 1;
    int align = 0, back = -1, width = -1, save_under = 0, redraw = 1;
    reg_t result = 0;
    Port old = *P();
    P()->pen_mode = 0;
    P()->pen = 0;
    P()->greyed = 0;
    while (i < argc) {
        reg_t cmd = ARG(i++);
        switch (cmd) {
        case 100: P()->cur_left = SARG(i); P()->cur_top = SARG(i + 1); i += 2; break;
        case 101: align = SARG(i++); break;
        case 102: P()->pen = (uint8_t)ARG(i++); break;
        case 103: back = SARG(i++); break;   /* -1 = no background */
        case 104: P()->greyed = ARG(i++) != 0; break;
        case 105: P()->font = SARG(i++); break;
        case 106: width = SARG(i++); break;
        case 107: save_under = 1; break;
        case 108: {
            uint16_t h = ARG(i++);
            SavedBits *b = bits_get(h);
            if (b) {
                Rect r = b->r;
                bits_restore(h);
                show_screen_rect(r);
            }
            return 0;
        }
        case 121: redraw = 0; break;
        default:
            break;
        }
    }
    Rect r;
    text_size(&r, buf, P()->font, width);
    rect_move(&r, P()->cur_left, P()->cur_top);
    locpanel_text(cur_port, buf, r.left + P()->left, r.top + P()->top, r.right + P()->left, r.bottom + P()->top);
    if (save_under) result = bits_save(r, MASK_VISUAL);
    if (back != -1) fill_rect(r, MASK_VISUAL, (uint8_t)back, 0, 0);
    text_box(buf, r, align, -1);
    if (!pic_not_valid && redraw) bits_show(r);
    int16_t ct = P()->cur_top, cl = P()->cur_left;
    *P() = old;
    P()->cur_top = ct;
    P()->cur_left = cl;
    return result;
}

/* ---------------------------------------------------------------- events */

/* SCI event queue: GetEvent returns the first queued event matching the
 * mask; events that don't match stay queued for a later call. */
#define EVQ 16
static PlatEvent evq[EVQ];
static int have_pending;   /* number of queued events */

static int next_event(uint16_t mask, PlatEvent *out, int peek)
{
    PlatEvent e;
    while (plat_poll_event(&e)) {
        if (have_pending == EVQ) {   /* full: drop the oldest */
            memmove(evq, evq + 1, sizeof evq[0] * (EVQ - 1));
            have_pending--;
        }
        evq[have_pending++] = e;
    }
    for (int i = 0; i < have_pending; i++) {
        if (!(evq[i].type & mask)) continue;
        *out = evq[i];
        if (!peek) {
            memmove(evq + i, evq + i + 1, sizeof evq[0] * (have_pending - i - 1));
            have_pending--;
        }
        return 1;
    }
    memset(out, 0, sizeof *out);
    plat_mouse(&out->x, &out->y);
    return 0;
}

extern void kernel_flush_disposals(void);

static reg_t kernel_get_event(int argc, uint16_t argv)
{
    uint16_t mask = ARG(0);
    reg_t obj = ARG(1);
    PlatEvent ev;
    kernel_flush_disposals();
    kgfx_present();
    int got = next_event(mask & EV_ALL, &ev, (mask & EV_PEEK) != 0);
    if (obj_is_object(obj)) {
        SS(obj, S_type, got ? ev.type : 0);
        SS(obj, S_message, got ? ev.message : 0);
        SS(obj, S_modifiers, ev.modifiers);
        SS(obj, S_x, ev.x);
        SS(obj, S_y, ev.y);
    }
    if (!got) {
        /* idle: give the host a chance to run */
        static int idle;
        if (++idle % 8 == 0) plat_sleep_ticks(0);
    }
    return (reg_t)got;
}

/* --------------------------------------------------------------- motion */

static reg_t kernel_can_be_here(reg_t o, reg_t list)
{
    Rect r = { GS(o, S_brTop), GS(o, S_brLeft), GS(o, S_brBottom), GS(o, S_brRight) };
    if (r.right < r.left || r.bottom < r.top) return 1;
    uint16_t sig = (uint16_t)GS(o, S_signal), mask = (uint16_t)GS(o, S_illegalBits);
    Rect c = r;
    rect_move(&c, ports[pic_port].left, ports[pic_port].top);
    rect_clip(&c, &SCREEN_RECT);
    uint16_t result = 0;
    for (int y = c.top; y < c.bottom; y++)
        for (int x = c.left; x < c.right; x++)
            result |= (uint16_t)(1 << (gfx.control[y * SCREEN_W + x] & 15));
    result &= mask;
    if (result) return 0;
    if (sig & (SIG_IGNOREACTOR | SIG_REMOVEVIEW)) return 1;
    for (reg_t n = list ? hp16(list) : 0; n; n = hp16((uint16_t)(n + 2))) {
        reg_t other = hp16((uint16_t)(n + 6));
        if (other == o) continue;
        uint16_t s2 = (uint16_t)GS(other, S_signal);
        if (s2 & (SIG_IGNOREACTOR | SIG_REMOVEVIEW | SIG_NOUPDATE)) continue;
        Rect q = { GS(other, S_brTop), GS(other, S_brLeft), GS(other, S_brBottom), GS(other, S_brRight) };
        if (q.right > r.left && q.left < r.right && q.bottom > r.top && q.top < r.bottom)
            return 0;
    }
    return 1;
}

static void kernel_init_bresen(reg_t mover, int step_factor)
{
    reg_t client = get_sel(mover, S_client);
    int mx = GS(mover, S_x), my = GS(mover, S_y);
    int xs = GS(client, S_xStep) * step_factor, ys = GS(client, S_yStep) * step_factor;
    int step = xs < ys ? ys * 2 : xs * 2;
    int dX = mx - GS(client, S_x), dY = my - GS(client, S_y);
    int dx = 0, dy = 0, i1 = 0, i2 = 0, di = 0, incr = 0, xaxis = 0;
    for (;;) {
        dx = xs; dy = ys; incr = 1;
        if (abs(dX) >= abs(dY)) {
            xaxis = 1;
            if (dX < 0) dx = -dx;
            dy = dX ? dx * dY / dX : 0;
            i1 = ((dx * dY) - (dy * dX)) * 2;
            if (dY < 0) { incr = -1; i1 = -i1; }
            i2 = i1 - (dX * 2);
            di = i1 - dX;
            if (dX < 0) { i1 = -i1; i2 = -i2; di = -di; }
        } else {
            xaxis = 0;
            if (dY < 0) dy = -dy;
            dx = dY ? dy * dX / dY : 0;
            i1 = ((dy * dX) - (dx * dY)) * 2;
            if (dX < 0) { incr = -1; i1 = -i1; }
            i2 = i1 - (dY * 2);
            di = i1 - dY;
            if (dY < 0) { i1 = -i1; i2 = -i2; di = -di; }
            break;
        }
        if (xs <= ys || !xs || ys >= abs(dy + incr)) break;
        if (!--step) break;
        xs--;
    }
    SS(mover, S_dx, dx); SS(mover, S_dy, dy);
    SS(mover, S_b_i1, i1); SS(mover, S_b_i2, i2); SS(mover, S_b_di, di);
    SS(mover, S_b_incr, incr); SS(mover, S_b_xAxis, xaxis);
}

static reg_t kernel_do_bresen(reg_t mover)
{
    reg_t client = get_sel(mover, S_client);
    int completed = 0;
    SS(client, S_signal, (uint16_t)GS(client, S_signal) & ~SIG_HITOBSTACLE);
    int handle_cnt = obj_find_var(mover, S_b_movCnt) >= 0;
    int cnt = 1, speed = 0;
    if (handle_cnt) {
        cnt = GS(mover, S_b_movCnt) + 1;
        speed = GS(client, S_moveSpeed);
    }
    if (speed < cnt) {
        cnt = 0;
        int cx = GS(client, S_x), cy = GS(client, S_y);
        int mx = GS(mover, S_x), my = GS(mover, S_y);
        int xaxis = GS(mover, S_b_xAxis), dx = GS(mover, S_dx), dy = GS(mover, S_dy);
        int incr = GS(mover, S_b_incr), i1 = GS(mover, S_b_i1), i2 = GS(mover, S_b_i2), di = GS(mover, S_b_di);
        int oi1 = i1, oi2 = i2, odi = di;
        SS(mover, S_xLast, cx);
        SS(mover, S_yLast, cy);
        int nv = obj_var_count(client);
        reg_t backup[256];
        for (int i = 0; i < nv && i < 256; i++) backup[i] = obj_var(client, i);
        if (xaxis) { if (abs(mx - cx) < abs(dx)) completed = 1; }
        else { if (abs(my - cy) < abs(dy)) completed = 1; }
        if (completed) { cx = mx; cy = my; }
        else {
            cx += dx; cy += dy;
            if (di < 0) di += i1;
            else { di += i2; if (xaxis == 0) cx += incr; else cy += incr; }
        }
        SS(client, S_x, cx);
        SS(client, S_y, cy);
        int collision = 0;
        if (S_cantBeHere >= 0 && has_sel(client, S_cantBeHere)) {
            if (invoke(client, S_cantBeHere, 0, NULL)) collision = 1;
        } else if (has_sel(client, S_canBeHere)) {
            if (!invoke(client, S_canBeHere, 0, NULL)) collision = 1;
        }
        if (collision) {
            for (int i = 0; i < nv && i < 256; i++) obj_set_var(client, i, backup[i]);
            i1 = oi1; i2 = oi2; di = odi;
            SS(client, S_signal, (uint16_t)GS(client, S_signal) | SIG_HITOBSTACLE);
        }
        SS(mover, S_b_i1, i1); SS(mover, S_b_i2, i2); SS(mover, S_b_di, di);
        if (handle_cnt) SS(mover, S_b_movCnt, cnt);
        if (cx == mx && cy == my) invoke(mover, S_moveDone, 0, NULL);
        return vm.acc;
    }
    if (handle_cnt) SS(mover, S_b_movCnt, cnt);
    return (reg_t)completed;
}

/* ---------------------------------------------------------------- palette */

static int palette_find(int r, int g, int b)
{
    int best = 0, bd = 1 << 30;
    for (int i = 0; i < 256; i++) {
        PalColor *c = &gfx.palette[i];
        int d = (c->r - r) * (c->r - r) + (c->g - g) * (c->g - g) + (c->b - b) * (c->b - b);
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

/* palette cycling: rotate entries [from,to) */
static uint32_t cycle_last[256];
static reg_t palette_animate(int from, int to, int speed)
{
    if (from < 0 || to > 256 || from >= to - 1) return 0;
    uint32_t now = plat_ticks();
    int aspeed = speed < 0 ? -speed : speed;
    if (now - cycle_last[from] < (uint32_t)aspeed) return 0;
    cycle_last[from] = now;
    PalColor tmp;
    if (speed > 0) {
        tmp = gfx.palette[from];
        memmove(&gfx.palette[from], &gfx.palette[from + 1], (to - from - 1) * sizeof(PalColor));
        gfx.palette[to - 1] = tmp;
    } else {
        tmp = gfx.palette[to - 1];
        memmove(&gfx.palette[from + 1], &gfx.palette[from], (to - from - 1) * sizeof(PalColor));
        gfx.palette[from] = tmp;
    }
    gfx.palette_dirty = 1;
    return 1;
}

static PalColor saved_palettes[4][256];
static int nsaved_palettes;

static reg_t kernel_palette(int argc, uint16_t argv)
{
    switch (ARG(0)) {
    case 1: {   /* set from resource */
        uint16_t sz;
        const uint8_t *d = res_load(RES_PALETTE, ARG(1), &sz);
        if (d) {
            if (argc > 2 && ARG(2) == 2) gfx_palette_from_data(d, 1);
            else { uint8_t map[256]; gfx_palette_merge(d, map); }
            res_unlock(RES_PALETTE, ARG(1));
        }
        return 0;
    }
    case 2: case 3: {           /* set / unset "used" flags: from, to, flag */
        int from = ARG(1), to = ARG(2), fl = argc > 3 ? ARG(3) : 1;
        if (to > 256) to = 256;
        for (int i = from < 0 ? 0 : from; i < to; i++) {
            if (ARG(0) == 2) gfx.palette[i].used |= (uint8_t)fl;
            else gfx.palette[i].used &= (uint8_t)~fl;
        }
        gfx.palette_gen++;
        return 0;
    }
    case 4: {   /* intensity */
        int from = ARG(1), to = ARG(2), in = ARG(3);
        if (to > 256) to = 256;
        for (int i = from; i < to; i++) gfx.intensity[i] = (uint8_t)(in > 100 ? 100 : in);
        gfx.palette_dirty = 1;
        if (argc < 5 || ARG(4)) kgfx_present();
        return 0;
    }
    case 5: return (reg_t)palette_find(ARG(1), ARG(2), ARG(3));
    case 6: {
        reg_t changed = 0;
        for (int i = 1; i + 2 < argc; i += 3)
            changed |= palette_animate(ARG(i), ARG(i + 1), SARG(i + 2));
        if (changed) kgfx_present();
        return changed;
    }
    case 7:
        if (nsaved_palettes < 4) {
            memcpy(saved_palettes[nsaved_palettes], gfx.palette, sizeof gfx.palette);
            return (reg_t)(0xc000 | nsaved_palettes++);
        }
        return 0;
    case 8: {
        int i = ARG(1) & 0xff;
        if ((ARG(1) & 0xff00) == 0xc000 && i < nsaved_palettes) {
            memcpy(gfx.palette, saved_palettes[i], sizeof gfx.palette);
            nsaved_palettes = i;
            gfx.palette_dirty = 1;
        }
        return 0;
    }
    }
    return 0;
}

/* ---------------------------------------------------------------- dispatch */

static Rect graph_rect(uint16_t argv)
{
    int16_t x = SARG(1), y = SARG(0), x1 = SARG(3), y1 = SARG(2);
    if (x > x1) { int16_t t = x; x = x1; x1 = t; }
    if (y > y1) { int16_t t = y; y = y1; y1 = t; }
    Rect r = { y, x, y1, x1 };
    return r;
}

static reg_t kernel_graph(int argc, uint16_t argv)
{
    int op = ARG(0);
    uint16_t a = (uint16_t)(argv + 2);   /* args after subop */
    if (getenv("GRAPHLOG")) {
        vm_log("Graph %d port %d (off %d,%d):", op, cur_port, P()->top, P()->left);
        for (int i = 1; i < argc; i++) vm_log(" %d", (int16_t)ARG(i));
        vm_log("\n");
    }
    switch (op) {
    case 2: return 256;
    case 4: {
        int16_t y0 = (int16_t)hp16(a), x0 = (int16_t)hp16((uint16_t)(a + 2));
        int16_t y1 = (int16_t)hp16((uint16_t)(a + 4)), x1 = (int16_t)hp16((uint16_t)(a + 6));
        int color = hp16((uint16_t)(a + 8));
        int pri = argc > 6 ? (int16_t)hp16((uint16_t)(a + 10)) : -1;
        int ctl = argc > 7 ? (int16_t)hp16((uint16_t)(a + 12)) : -1;
        gfx_draw_line(x0 + P()->left, y0 + P()->top, x1 + P()->left, y1 + P()->top,
                      (uint8_t)color, (uint8_t)(pri < 0 ? 255 : pri), (uint8_t)(ctl < 0 ? 255 : ctl));
        return 0;
    }
    case 7: { Rect r = graph_rect(a); return bits_save(r, hp16((uint16_t)(a + 8))); }
    case 8: {
        uint16_t h = hp16(a);
        bits_restore(h);
        return 0;
    }
    case 9: { Rect r = graph_rect(a); fill_rect(r, MASK_VISUAL, P()->back, 0, 0); return 0; }
    case 10: { Rect r = graph_rect(a); fill_rect(r, MASK_VISUAL, P()->pen, 0, 0); return 0; }
    case 11: {
        Rect r = graph_rect(a);
        int mask = hp16((uint16_t)(a + 8)), color = hp16((uint16_t)(a + 10));
        int pri = argc > 7 ? hp16((uint16_t)(a + 12)) : 0, ctl = argc > 8 ? hp16((uint16_t)(a + 14)) : 0;
        fill_rect(r, mask, (uint8_t)color, (uint8_t)pri, (uint8_t)ctl);
        return 0;
    }
    case 12: { Rect r = graph_rect(a); bits_show(r); kgfx_present(); return 0; }
    case 13: { Rect r = graph_rect(a); offset_rect(&r); redraw_cast_box(r); return 0; }
    case 14: gfx_set_priority_bands(hp16(a), hp16((uint16_t)(a + 2))); return 0;
    }
    return 0;
}

static void get_text_rect(uint16_t dest, const char *text, int font, int maxw)
{
    Rect r = { 0, 0, 0, 0 };
    if (*text) text_size(&r, text, font, maxw);
    hw16(dest, (reg_t)r.top);
    hw16((uint16_t)(dest + 2), (reg_t)r.left);
    hw16((uint16_t)(dest + 4), (reg_t)r.bottom);
    hw16((uint16_t)(dest + 6), (reg_t)r.right);
}

static reg_t kernel_edit_control(reg_t o, reg_t ev)
{
    if (!obj_is_object(o)) return 0;
    if (GS(o, S_type) != CTL_EDIT || !obj_is_object(ev)) { draw_control(o, 0); return 0; }
    int type = GS(ev, S_type), key = GS(ev, S_message);
    reg_t t = get_sel(o, S_text);
    if (!t) return 0;
    char *s = hstr(t);
    int len = (int)strlen(s), cur = GS(o, S_cursor), max = GS(o, S_max);
    if (cur > len) cur = len;
    if (type == EV_KEYDOWN) {
        int handled = 1;
        if (key == 8) {                       /* backspace */
            if (cur > 0) { memmove(s + cur - 1, s + cur, len - cur + 1); cur--; }
        } else if (key == 0x4b00) { if (cur > 0) cur--; }
        else if (key == 0x4d00) { if (cur < len) cur++; }
        else if (key == 0x4700) cur = 0;
        else if (key == 0x4f00) cur = len;
        else if (key == 0x5300) { if (cur < len) memmove(s + cur, s + cur + 1, len - cur); }
        else if (key >= 32 && key < 256 && len < max) {
            memmove(s + cur + 1, s + cur, len - cur + 1);
            s[cur++] = (char)key;
        } else handled = 0;
        if (handled) SS(ev, S_type, 0);   /* claim the event */
    }
    SS(o, S_cursor, cur);
    draw_control(o, 0);
    return 0;
}

void kgfx_init(void)
{
    init_selectors();
    default_font = 0;
    {   /* system palette */
        uint16_t sz;
        const uint8_t *pal = res_load(RES_PALETTE, 999, &sz);
        if (pal) { gfx_palette_from_data(pal, 1); res_unlock(RES_PALETTE, 999); }
    }
    ports_init();
    pic_not_valid = 0;
    ncast = 0;
    last_cast_list = 0;
    have_pending = 0;
    nsaved_palettes = 0;
}

/* After a restore: the heap was replaced, so drop every C-side reference
 * into it. Windows, saved bits and the cast are rebuilt by Game::replay. */
void kgfx_after_restore(void)
{
    ports_init();
    pic_not_valid = 1;
    ncast = 0;
    last_cast_list = 0;
    have_pending = 0;
    nsaved_palettes = 0;
    memset(cycle_last, 0, sizeof cycle_last);
}

reg_t kgfx_call(int num, int argc, uint16_t argv, int *handled)
{
    *handled = 1;
    switch (num) {
    case 0x08: {   /* DrawPic(pic, flags, clear, epalette) */
        int flags = argc > 1 ? ARG(1) : 0;
        int add_to = 0;
        if (argc > 2) add_to = !(ARG(2) != 0);
        int old = set_port(pic_port);
        pic_not_valid = 1;
        draw_picture(ARG(0), (flags & 0x4000) != 0, add_to);
        set_port(old);
        return 0;
    }
    case 0x09: pic_not_valid = 2; return 0;            /* Show */
    case 0x0a: {                                       /* PicNotValid */
        int old = pic_not_valid;
        if (argc > 0) pic_not_valid = ARG(0);
        return (reg_t)old;
    }
    case 0x0b: kernel_animate(argc > 0 ? ARG(0) : 0, argc > 1 ? ARG(1) : 0, argc, argv); return 0;
    case 0x0c: {                                       /* SetNowSeen */
        reg_t o = ARG(0);
        Rect r;
        cel_rect(GS(o, S_view), GS(o, S_loop), GS(o, S_cel), GS(o, S_x), GS(o, S_y),
                 S_z >= 0 ? GS(o, S_z) : 0, &r);
        set_ns_rect(o, &r);
        return 0;
    }
    case 0x0d: {
        uint16_t sz;
        const uint8_t *v = view_get(GS(ARG(0), S_view), &sz);
        return v ? (reg_t)view_loop_count(v) : 0;
    }
    case 0x0e: {
        uint16_t sz;
        reg_t o = ARG(0);
        const uint8_t *v = view_get(GS(o, S_view), &sz);
        return v ? (reg_t)view_cel_count(v, GS(o, S_loop)) : 0;
    }
    case 0x0f: case 0x10: {                            /* CelWide / CelHigh */
        uint16_t sz;
        const uint8_t *v = view_get(ARG(0), &sz);
        CelInfo c;
        if (!v || !view_get_cel(v, SARG(1), SARG(2), &c, NULL)) return 0;
        return num == 0x0f ? c.width : c.height;
    }
    case 0x11: {                                       /* DrawCel(view,loop,cel,x,y,pri) */
        Rect r = { SARG(4), SARG(3), 0, 0 };
        int pri = argc > 5 ? SARG(5) : -1;
        uint16_t sz;
        const uint8_t *v = view_get(ARG(0), &sz);
        CelInfo c;
        if (v && view_get_cel(v, SARG(1), SARG(2), &c, NULL)) {
            draw_cel(ARG(0), SARG(1), SARG(2), r, pri == -1 ? 255 : pri);
            r.bottom = (int16_t)(r.top + c.height);
            r.right = (int16_t)(r.left + c.width);
            if (!pic_not_valid) bits_show(r);
        }
        return 0;
    }
    case 0x12:                                         /* AddToPic */
        if (argc == 1) add_to_pic_list(ARG(0));
        else {
            int old = set_port(pic_port);
            Rect r;
            int pri = argc > 5 ? SARG(5) : -1;
            if (pri == -1) pri = coord_to_priority(SARG(4));
            cel_rect(ARG(0), SARG(1), SARG(2), SARG(3), SARG(4), 0, &r);
            draw_cel(ARG(0), SARG(1), SARG(2), r, pri);
            if (argc <= 6 || SARG(6) != -1) {
                Rect c = r;
                int t = priority_to_coord(pri) - 1;
                if (t < c.top) t = c.top;
                if (t > c.bottom - 1) t = c.bottom - 1;
                c.top = (int16_t)t;
                fill_rect(c, MASK_CONTROL, 0, 0, argc > 6 ? (uint8_t)ARG(6) : 15);
            }
            pic_not_valid = 1;
            set_port(old);
        }
        return 0;
    case 0x13: {                                       /* NewWindow */
        Rect r = { SARG(0), SARG(1), SARG(2), SARG(3) };
        int extra = argc >= 13 ? 4 : 0;
        Rect rr, *prr = NULL;
        if (extra) { rr = (Rect){ SARG(4), SARG(5), SARG(6), SARG(7) }; prr = &rr; }
        reg_t tref = ARG(4 + extra);
        const char *title = (tref >= HEAP_START) ? hstr(tref) : NULL;
        int style = SARG(5 + extra);
        int pri = argc > 6 + extra ? SARG(6 + extra) : -1;
        int pen = argc > 7 + extra ? SARG(7 + extra) : 0;
        int back = argc > 8 + extra ? SARG(8 + extra) : 255;
        if (getenv("GRAPHLOG")) vm_log("NewWindow %d,%d,%d,%d style %x title %s by %s\n", r.top, r.left, r.bottom, r.right, style, title ? title : "-", obj_name(vm_self));
        return (reg_t)new_window(r, prr, title, style, pri, (uint8_t)pen, (uint8_t)back);
    }
    case 0x14: return (reg_t)cur_port;                 /* GetPort */
    case 0x15:                                         /* SetPort */
        if (argc == 1) {
            int id = ARG(0);
            if (id == 0) set_port(wmgr_port);
            else if (id != 0xffff) set_port(id);
        } else if (argc >= 6) {
            Port *pw = &ports[pic_port];
            pw->rect = (Rect){ SARG(0), SARG(1), SARG(2), SARG(3) };
            pw->top = SARG(4);
            pw->left = SARG(5);
            rect_move(&pw->rect, 0, 0);
            if (argc >= 7) gfx_set_priority_bands(42, 200);
        }
        return 0;
    case 0x16: dispose_window(ARG(0), argc > 1 && ARG(1)); return 0;
    case 0x17: draw_control(ARG(0), 0); return 0;
    case 0x18: draw_control(ARG(0), 1); return 0;
    case 0x19: return kernel_edit_control(ARG(0), ARG(1));
    case 0x1a: {                                       /* TextSize(rect, text, font, width) */
        const char *t = vm_string(ARG(1), 0, NULL);
        get_text_rect(ARG(0), t, argc > 2 ? SARG(2) : P()->font, argc > 3 ? SARG(3) : 0);
        return 0;
    }
    case 0x1b: return kernel_display(argc, argv);
    case 0x1c: return kernel_get_event(argc, argv);
    case 0x1d: case 0x1e: {                            /* GlobalToLocal / LocalToGlobal */
        reg_t o = ARG(0);
        int s = num == 0x1d ? -1 : 1;
        SS(o, S_x, GS(o, S_x) + s * P()->left);
        SS(o, S_y, GS(o, S_y) + s * P()->top);
        return 0;
    }
    case 0x1f: {                                       /* MapKeyToDir */
        reg_t o = ARG(0);
        if (GS(o, S_type) == EV_KEYDOWN) {
            static const struct { uint16_t key; uint16_t dir; } map[] = {
                { 0x4800, 1 }, { 0x4900, 2 }, { 0x4d00, 3 }, { 0x5100, 4 },
                { 0x5000, 5 }, { 0x4f00, 6 }, { 0x4b00, 7 }, { 0x4700, 8 }, { 0x4c00, 0 },
            };
            uint16_t k = (uint16_t)GS(o, S_message);
            for (unsigned i = 0; i < sizeof map / sizeof *map; i++)
                if (map[i].key == k) {
                    SS(o, S_type, EV_JOYSTICK);
                    SS(o, S_message, map[i].dir);
                    break;
                }
        }
        return o;
    }
    case 0x20: case 0x23: return 0;                    /* DrawMenuBar, DrawStatus */
    case 0x24: case 0x25: case 0x26: return 0;         /* Parse, Said, SetSynonyms */
    case 0x27: return 1;                               /* HaveMouse */
    case 0x28: {                                       /* SetCursor */
        int id = SARG(0);
        if (argc >= 4) { /* position: not supported on touch */ }
        if (argc >= 2 && SARG(1) == 0) id = -1;
        set_cursor_shape(id);
        return 0;
    }
    case 0x2d: {                                       /* DoSound */
        extern reg_t ksound_call(int argc, uint16_t argv);
        return ksound_call(argc, argv);
    }
    case 0x4b: {                                       /* BaseSetter */
        reg_t o = ARG(0);
        if (obj_find_var(o, S_brLeft) < 0) return 0;
        Rect r;
        int y = GS(o, S_y);
        cel_rect(GS(o, S_view), GS(o, S_loop), GS(o, S_cel), GS(o, S_x), y,
                 S_z >= 0 ? GS(o, S_z) : 0, &r);
        r.bottom = (int16_t)(y + 1);
        r.top = (int16_t)(r.bottom - GS(o, S_yStep));
        SS(o, S_brLeft, r.left); SS(o, S_brRight, r.right);
        SS(o, S_brTop, r.top); SS(o, S_brBottom, r.bottom);
        return 0;
    }
    case 0x4c: {                                       /* DirLoop */
        reg_t o = ARG(0);
        int angle = ARG(1);
        if ((uint16_t)GS(o, S_signal) & SIG_DOESNTTURN) return 0;
        int use = -1;
        if (angle > 315 || angle < 45) use = 3;
        else if (angle > 135 && angle < 225) use = 2;
        if (use == -1) use = angle >= 180 ? 1 : 0;
        else {
            uint16_t sz;
            const uint8_t *v = view_get(GS(o, S_view), &sz);
            if (!v || view_loop_count(v) < 4) return 0;
        }
        SS(o, S_loop, use);
        return 0;
    }
    case 0x4d: return kernel_can_be_here(ARG(0), argc > 1 ? ARG(1) : 0);
    case 0x4e: {                                       /* OnControl */
        int mask = MASK_CONTROL, a = 0;
        if (!(argc == 2 || argc == 4)) { mask = ARG(0); a = 1; }
        Rect r;
        r.left = SARG(a); r.top = SARG(a + 1);
        if (argc > 3) { r.right = SARG(a + 2); r.bottom = SARG(a + 3); }
        else { r.right = (int16_t)(r.left + 1); r.bottom = (int16_t)(r.top + 1); }
        rect_move(&r, ports[pic_port].left, ports[pic_port].top);
        rect_clip(&r, &SCREEN_RECT);
        uint16_t res = 0;
        for (int y = r.top; y < r.bottom; y++)
            for (int x = r.left; x < r.right; x++) {
                int o = y * SCREEN_W + x;
                if (mask & MASK_VISUAL) res |= (uint16_t)(1 << (gfx.visual[o] & 15));
                if (mask & MASK_PRIORITY) res |= (uint16_t)(1 << (gfx.priority[o] & 15));
                if (mask & MASK_CONTROL) res |= (uint16_t)(1 << (gfx.control[o] & 15));
            }
        return res;
    }
    case 0x4f: kernel_init_bresen(ARG(0), argc > 1 ? SARG(1) : 1); return 0;
    case 0x50: return kernel_do_bresen(ARG(0));
    case 0x51: return 0;                               /* DoAvoider */
    case 0x61:                                         /* CoordPri */
        if (argc < 2 || SARG(0) != 1) return (reg_t)coord_to_priority(SARG(0));
        return (reg_t)priority_to_coord(SARG(1));
    case 0x66:                                         /* ShakeScreen */
        kgfx_present();
        return 0;
    case 0x6c: return kernel_graph(argc, argv);
    case 0x6d: return 0;                               /* Joystick */
    case 0x6e: return 0;                               /* ShiftScreen */
    case 0x6f: return kernel_palette(argc, argv);
    }
    *handled = 0;
    return 0;
}
