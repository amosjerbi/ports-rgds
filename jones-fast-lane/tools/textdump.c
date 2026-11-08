/* textdump <gamedir> <module>: print text resource lines, one per line
 * (newlines inside a line become \n). Used by tools/mksubs.py. */
#include <stdio.h>
#include <stdlib.h>
#include "../src/engine/resource.h"

static uint8_t *slurp(const char *path, uint32_t *size)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(n ? n : 1);
    *size = (uint32_t)fread(b, 1, n, f);
    fclose(f);
    return b;
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: %s gamedir module\n", argv[0]); return 2; }
    char path[1024];
    uint32_t msize;
    snprintf(path, sizeof path, "%s/resource.map", argv[1]);
    uint8_t *map = slurp(path, &msize);
    if (!map) { perror(path); return 1; }
    ResVolume vols[RES_MAX_VOLUMES] = {0};
    int n = 0;
    for (int i = 1; i <= RES_MAX_VOLUMES; i++) {
        snprintf(path, sizeof path, "%s/resource.%03d", argv[1], i);
        uint8_t *d = slurp(path, &vols[n].size);
        if (!d) break;
        vols[n++].data = d;
    }
    res_init(map, msize, vols, n);
    uint16_t sz;
    const uint8_t *t = res_load(RES_TEXT, atoi(argv[2]), &sz);
    if (!t) return 1;
    for (uint16_t i = 0; i < sz; i++) {
        if (t[i] == 0) putchar('\n');
        else if (t[i] == '\n') fputs("\\n", stdout);
        else putchar(t[i]);
    }
    return 0;
}
