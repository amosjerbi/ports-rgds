/* Render a sound resource's AdLib track to a WAV file (Mac test tool).
 * usage: musicwav <gamedir> <sound#> <seconds> <out.wav> */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/engine/resource.h"
#include "../src/engine/music.h"

static uint8_t *slurp(const char *path, uint32_t *size)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(n);
    if (fread(b, 1, n, f) != (size_t)n) { fclose(f); free(b); return NULL; }
    fclose(f); *size = (uint32_t)n; return b;
}

static void w32(FILE *f, uint32_t v) { fwrite(&v, 4, 1, f); }
static void w16(FILE *f, uint16_t v) { fwrite(&v, 2, 1, f); }

int main(int argc, char **argv)
{
    if (argc < 5) { fprintf(stderr, "usage: %s gamedir sound# seconds out.wav\n", argv[0]); return 2; }
    char path[1024]; uint32_t msz;
    snprintf(path, sizeof path, "%s/resource.map", argv[1]);
    uint8_t *map = slurp(path, &msz);
    ResVolume vols[RES_MAX_VOLUMES]; int nv = 0;
    for (int i = 1; i <= RES_MAX_VOLUMES; i++) {
        snprintf(path, sizeof path, "%s/resource.%03d", argv[1], i);
        uint8_t *d = slurp(path, &vols[nv].size);
        if (!d) break;
        vols[nv++].data = d;
    }
    res_init(map, msz, vols, nv);
    uint16_t psz, ssz;
    const uint8_t *patch = res_load(RES_PATCH, 3, &psz);
    const uint8_t *snd = res_load(RES_SOUND, atoi(argv[2]), &ssz);
    if (!snd) { fprintf(stderr, "no sound %s\n", argv[2]); return 1; }
    int rate = 22050;
    printf("instruments: %d\n", music_init(rate, patch, psz));
    printf("play: %d\n", music_play(1, snd, ssz, 0, 127));
    int n = atoi(argv[3]) * rate;
    int16_t *pcm = malloc(n * 2);
    music_render(pcm, n);
    FILE *f = fopen(argv[4], "wb");
    fwrite("RIFF", 1, 4, f); w32(f, 36 + n * 2); fwrite("WAVEfmt ", 1, 8, f);
    w32(f, 16); w16(f, 1); w16(f, 1); w32(f, rate); w32(f, rate * 2); w16(f, 2); w16(f, 16);
    fwrite("data", 1, 4, f); w32(f, n * 2); fwrite(pcm, 2, n, f); fclose(f);
    int peak = 0; for (int i = 0; i < n; i++) if (abs(pcm[i]) > peak) peak = abs(pcm[i]);
    printf("peak %d\n", peak);
    return 0;
}
