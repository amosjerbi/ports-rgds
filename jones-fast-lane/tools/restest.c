/* macOS test: load every resource, verify decompression, optionally dump.
 * usage: restest <gamedir> [dumpdir] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/engine/resource.h"

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

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s gamedir [dumpdir]\n", argv[0]); return 2; }
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
    printf("%d resources in %d volumes\n", n, nvols);
    static const char *algo_names[] = { "none", "LZW", "Huffman", "LZW1" };
    for (int m = 0; m < 5; m++)
        printf("  method %d -> %s\n", m, algo_names[res_method_algo(m)]);

    int ok = 0, bad = 0;
    for (int i = 0; i < n; i++) {
        ResEntry *e = res_entry(i);
        uint16_t sz;
        const uint8_t *d = res_load(e->type, e->number, &sz);
        if (!d) {
            printf("FAIL %s.%03d method %d packed %u size %u\n",
                   res_type_name(e->type), e->number, e->method, e->packed_size, e->size);
            bad++;
            continue;
        }
        ok++;
        if (argc > 2) {
            snprintf(path, sizeof path, "%s/%s.%03d", argv[2], res_type_name(e->type), e->number);
            FILE *f = fopen(path, "wb");
            if (f) { fwrite(d, 1, sz, f); fclose(f); }
        }
        res_unlock(e->type, e->number);
    }
    res_purge();
    printf("ok %d, failed %d\n", ok, bad);
    return bad ? 1 : 0;
}
