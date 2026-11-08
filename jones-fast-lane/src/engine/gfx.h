/* SCI1 VGA graphics: 320x200 visual/priority/control screens, palette,
 * pic (vector + embedded bitmap) rendering, views/cels, fonts.
 * Portable C, no platform dependencies. */
#ifndef SCI_GFX_H
#define SCI_GFX_H

#include <stdint.h>

#define SCREEN_W 320
#define SCREEN_H 200

#define MASK_VISUAL   1
#define MASK_PRIORITY 2
#define MASK_CONTROL  4

typedef struct { uint8_t used, r, g, b; } PalColor;

typedef struct {
    uint8_t  visual[SCREEN_W * SCREEN_H];
    uint8_t  priority[SCREEN_W * SCREEN_H];
    uint8_t  control[SCREEN_W * SCREEN_H];
    uint8_t  display[SCREEN_W * SCREEN_H];   /* what is actually on screen */
    int      display_dirty;
    PalColor palette[256];
    uint8_t  intensity[256];                 /* percent, for palette fades */
    int      palette_dirty;
    uint32_t palette_gen;                    /* bumped whenever the palette changes */
    uint8_t  priority_bands[200];   /* y -> priority */
} GfxState;

extern GfxState gfx;

typedef struct {
    int pics, opcodes[16], ext_opcodes[16], patterns, fills, lines, embedded;
    int unknown;
} PicStats;
extern PicStats pic_stats;

void gfx_init(void);
void gfx_clear(uint8_t visual, uint8_t priority, uint8_t control);

/* Palette */
void gfx_palette_from_data(const uint8_t *data, int force);  /* SCI1 palette block */
void gfx_set_priority_bands(int top, int bottom);

/* Pics. Draws at (0, top) in screen coords; clear = start from blank screens. */
int  gfx_draw_pic(const uint8_t *data, uint32_t size, int top, int clear, int mirrored);

/* Views */
typedef struct {
    uint16_t width, height;
    int8_t   dx;       /* displacement */
    uint8_t  dy;
    uint8_t  clear_key;
    const uint8_t *rle;
} CelInfo;

int  view_loop_count(const uint8_t *view);
int  view_cel_count(const uint8_t *view, int loop);
int  view_get_cel(const uint8_t *view, int loop, int cel, CelInfo *out, int *mirrored);
const uint8_t *view_apply_palette(const uint8_t *view, uint32_t size);   /* colour map or NULL */
void gfx_palette_merge(const uint8_t *data, uint8_t *map);
/* Decode a cel into w*h bytes (clear_key where transparent). */
void cel_decode(const CelInfo *cel, uint8_t *out);
/* Draw cel with its bottom-center at (x,y) like SCI's DrawCel/actors.
 * priority 0xff = don't test/write priority. */
void gfx_draw_cel(const uint8_t *view, int loop, int cel, int x, int y, int priority);
/* Draw cel with top-left at (x,y), no displacement. */
void gfx_draw_cel_raw(const CelInfo *cel, int mirrored, int x, int y, int priority);

/* Fonts */
int  font_height(const uint8_t *font);
int  font_char_width(const uint8_t *font, int ch);
int  font_text_width(const uint8_t *font, const char *s);
void gfx_draw_text(const uint8_t *font, const char *s, int x, int y, uint8_t color);

/* Primitives */
void gfx_fill_rect(int x, int y, int w, int h, uint8_t color);
void gfx_draw_line(int x0, int y0, int x1, int y1, uint8_t color, uint8_t prio, uint8_t ctrl);

#endif
