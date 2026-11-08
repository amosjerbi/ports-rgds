/* macOS test: render pics, view sheets and fonts to BMP files.
 * usage: gfxtest <gamedir> <outdir> */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/engine/resource.h"
#include "../src/engine/gfx.h"

static uint8_t *slurp(const char *path, uint32_t *size)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc(n ? n : 1);
    if (fread(buf, 1, n, f) != (size_t)n) { free(buf); fclose(f); return NULL; }
    fclose(f);
    *size = (uint32_t)n;
    return buf;
}

static void put32(FILE *f, uint32_t v) { fputc(v, f); fputc(v >> 8, f); fputc(v >> 16, f); fputc(v >> 24, f); }
static void put16(FILE *f, uint16_t v) { fputc(v, f); fputc(v >> 8, f); }

/* write an 8-bit BMP of buf (w x h) using the current gfx palette */
static void write_bmp(const char *path, const uint8_t *buf, int w, int h)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return; }
    int stride = (w + 3) & ~3;
    uint32_t data_off = 14 + 40 + 1024;
    fputc('B', f); fputc('M', f);
    put32(f, data_off + stride * h); put32(f, 0); put32(f, data_off);
    put32(f, 40); put32(f, w); put32(f, h); put16(f, 1); put16(f, 8);
    put32(f, 0); put32(f, stride * h); put32(f, 2835); put32(f, 2835); put32(f, 256); put32(f, 0);
    for (int i = 0; i < 256; i++) {
        fputc(gfx.palette[i].b, f); fputc(gfx.palette[i].g, f); fputc(gfx.palette[i].r, f); fputc(0, f);
    }
    for (int y = h - 1; y >= 0; y--) {
        fwrite(buf + y * w, 1, w, f);
        for (int p = w; p < stride; p++) fputc(0, f);
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: %s gamedir outdir\n", argv[0]); return 2; }
    char path[1024];
    uint32_t msize;
    snprintf(path, sizeof path, "%s/resource.map", argv[1]);
    uint8_t *map = slurp(path, &msize);
    if (!map) { perror(path); return 1; }
    ResVolume vols[RES_MAX_VOLUMES];
    int nvols = 0;
    for (int i = 1; i <= RES_MAX_VOLUMES; i++) {
        snprintf(path, sizeof path, "%s/resource.%03d", argv[1], i);
        uint8_t *d = slurp(path, &vols[nvols].size);
        if (!d) break;
        vols[nvols++].data = d;
    }
    int n = res_init(map, msize, vols, nvols);
    gfx_init();

    /* pics */
    for (int i = 0; i < n; i++) {
        ResEntry *e = res_entry(i);
        if (e->type != RES_PIC) continue;
        uint16_t sz;
        const uint8_t *d = res_load(RES_PIC, e->number, &sz);
        int before = pic_stats.unknown;
        int rc = gfx_draw_pic(d, sz, 0, 1, 0);
        printf("pic.%03d size %u rc %d unknown %d\n", e->number, sz, rc, pic_stats.unknown - before);
        snprintf(path, sizeof path, "%s/pic_%03d.bmp", argv[2], e->number);
        write_bmp(path, gfx.visual, SCREEN_W, SCREEN_H);
        snprintf(path, sizeof path, "%s/pic_%03d_pri.bmp", argv[2], e->number);
        static uint8_t pri[SCREEN_W * SCREEN_H];
        for (int k = 0; k < SCREEN_W * SCREEN_H; k++) pri[k] = (uint8_t)(gfx.priority[k] * 16);
        write_bmp(path, pri, SCREEN_W, SCREEN_H);
    }
    printf("opcodes f0..ff:");
    for (int k = 0; k < 16; k++) printf(" %x:%d", k, pic_stats.opcodes[k]);
    printf("\next:");
    for (int k = 0; k < 16; k++) if (pic_stats.ext_opcodes[k]) printf(" %d:%d", k, pic_stats.ext_opcodes[k]);
    printf("\npatterns %d fills %d lines %d embedded %d\n", pic_stats.patterns, pic_stats.fills,
           pic_stats.lines, pic_stats.embedded);

    /* view sheets: each view on a fresh canvas using the base palette from pic 0 + its own */
    uint16_t psz;
    const uint8_t *pal = res_load(RES_PALETTE, 999, &psz);
    for (int i = 0; i < n; i++) {
        ResEntry *e = res_entry(i);
        if (e->type != RES_VIEW) continue;
        uint16_t sz;
        const uint8_t *v = res_load(RES_VIEW, e->number, &sz);
        if (pal) gfx_palette_from_data(pal, 1);
        view_apply_palette(v, sz);
        gfx_clear(0, 0, 0);
        /* lay out cels row per loop */
        int y = 2;
        for (int l = 0; l < view_loop_count(v) && y < SCREEN_H; l++) {
            int x = 2, rowh = 0;
            for (int c = 0; c < view_cel_count(v, l); c++) {
                CelInfo ci; int mir;
                view_get_cel(v, l, c, &ci, &mir);
                if (x + ci.width > SCREEN_W) { x = 2; y += rowh + 2; rowh = 0; }
                gfx_draw_cel_raw(&ci, mir, x, y, 0xff);
                x += ci.width + 2;
                if (ci.height > rowh) rowh = ci.height;
            }
            y += rowh + 2;
        }
        snprintf(path, sizeof path, "%s/view_%03d.bmp", argv[2], e->number);
        write_bmp(path, gfx.visual, SCREEN_W, SCREEN_H);
    }

    /* fonts */
    if (pal) gfx_palette_from_data(pal, 1);
    gfx_clear(255, 0, 0);
    int y = 2;
    for (int i = 0; i < n; i++) {
        ResEntry *e = res_entry(i);
        if (e->type != RES_FONT) continue;
        uint16_t sz;
        const uint8_t *f = res_load(RES_FONT, e->number, &sz);
        char line[64];
        snprintf(line, sizeof line, "Font %d: Jones in the Fast Lane 0123", e->number);
        gfx_draw_text(f, line, 2, y, 0);
        y += font_height(f) + 2;
    }
    snprintf(path, sizeof path, "%s/fonts.bmp", argv[2]);
    write_bmp(path, gfx.visual, SCREEN_W, SCREEN_H);
    return 0;
}
