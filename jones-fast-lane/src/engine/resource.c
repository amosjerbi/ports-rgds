/* SCI resource manager: parses resource.map, reads volume headers,
 * decompresses and caches resources. */
#include <stdlib.h>
#include <string.h>
#include "resource.h"

static ResEntry  *g_entries;
static int        g_count;
static ResVolume  g_vols[RES_MAX_VOLUMES];
static int        g_nvols;
static CompAlgo   g_algo[8];
static uint32_t   g_stamp, g_cache_bytes, g_budget;

void res_set_cache_budget(uint32_t bytes) { g_budget = bytes; }
uint32_t res_cache_bytes(void) { return g_cache_bytes; }

/* Evict least-recently-used unlocked data until `need` more bytes fit.
 * The 8 most recently used entries are kept: callers may still hold them. */
static void make_room(uint32_t need)
{
    while (g_budget && g_cache_bytes + need > g_budget) {
        ResEntry *victim = NULL;
        for (int i = 0; i < g_count; i++) {
            ResEntry *e = &g_entries[i];
            if (!e->data || e->locks || g_stamp - e->stamp < 8) continue;
            if (!victim || e->stamp < victim->stamp) victim = e;
        }
        if (!victim) break;
        free(victim->data);
        victim->data = NULL;
        g_cache_bytes -= victim->size;
    }
}

/* malloc, evicting unlocked cached resources when memory runs out */
static void *alloc_evict(uint32_t n)
{
    for (;;) {
        void *p = malloc(n ? n : 1);
        if (p) return p;
        ResEntry *victim = NULL;
        for (int i = 0; i < g_count; i++) {
            ResEntry *e = &g_entries[i];
            if (!e->data || e->locks || g_stamp - e->stamp < 8) continue;
            if (!victim || e->stamp < victim->stamp) victim = e;
        }
        if (!victim) return NULL;
        free(victim->data);
        victim->data = NULL;
        g_cache_bytes -= victim->size;
    }
}

void *res_alloc(uint32_t n) { return alloc_evict(n); }

static int vol_read(const ResVolume *v, uint32_t off, void *buf, uint32_t n)
{
    if (off + n > v->size) return 0;
    if (v->data) { memcpy(buf, v->data + off, n); return 1; }
    return v->read && v->read(v->user, off, buf, n);
}

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t rd32(const uint8_t *p) { return rd16(p) | (uint32_t)rd16(p + 2) << 16; }

static const char *const k_type_names[] = {
    "view", "pic", "script", "text", "sound", "memory", "vocab", "font",
    "cursor", "patch", "bitmap", "palette", "cdaudio", "audio", "sync",
    "message", "map", "heap"
};

const char *res_type_name(int type)
{
    if (type >= 0 && type < (int)(sizeof k_type_names / sizeof *k_type_names))
        return k_type_names[type];
    return "unknown";
}

CompAlgo res_method_algo(int method)
{
    return (method >= 0 && method < 8) ? g_algo[method] : COMP_NONE;
}

/* Decompress entry into a fresh buffer; returns NULL on failure. */
static uint8_t *decode_entry(const ResEntry *e, CompAlgo algo)
{
    const ResVolume *v = &g_vols[e->volume - 1];
    uint8_t *dst = alloc_evict(e->size);
    if (!dst)
        return NULL;
    int n;
    if (v->data) {
        n = res_decompress(algo, v->data + e->offset + 8, e->packed_size, dst, e->size);
    } else if (algo == COMP_NONE && e->packed_size == e->size) {
        n = vol_read(v, e->offset + 8, dst, e->size) ? e->size : -1;
    } else {
        uint8_t *src = alloc_evict(e->packed_size);
        n = src && vol_read(v, e->offset + 8, src, e->packed_size)
            ? res_decompress(algo, src, e->packed_size, dst, e->size) : -1;
        free(src);
    }
    if (n != e->size) {
        free(dst);
        return NULL;
    }
    return dst;
}

/* The meaning of method ids changed between SCI0 and SCI1. Pick, per method,
 * the algorithm that decodes the first resource using it to its exact size. */
static void detect_methods(void)
{
    static const CompAlgo candidates[] = { COMP_LZW, COMP_HUFFMAN, COMP_LZW1 };
    g_algo[0] = COMP_NONE;
    for (int m = 1; m < 8; m++) {
        g_algo[m] = (m == 1) ? COMP_LZW : (m == 2) ? COMP_HUFFMAN : COMP_LZW1;
        for (int i = 0; i < g_count; i++) {
            if (g_entries[i].method != m)
                continue;
            for (unsigned c = 0; c < sizeof candidates / sizeof *candidates; c++) {
                uint8_t *d = decode_entry(&g_entries[i], candidates[c]);
                if (d) {
                    free(d);
                    g_algo[m] = candidates[c];
                    break;
                }
            }
            break;
        }
    }
}

int res_init(const uint8_t *map, uint32_t map_size, const ResVolume *vols, int nvols)
{
    if (nvols > RES_MAX_VOLUMES)
        return -1;
    res_shutdown();
    memcpy(g_vols, vols, sizeof *vols * nvols);
    g_nvols = nvols;

    int cap = (int)(map_size / 6);
    g_entries = calloc(cap ? cap : 1, sizeof *g_entries);
    if (!g_entries)
        return -1;

    /* Floppy maps pack vol:6/offset:26, the CD map vol:4/offset:28. */
    for (int shift = 26; shift <= 28 && !g_count; shift += 2)
    for (uint32_t i = 0; i + 6 <= map_size; i += 6) {
        uint16_t id = rd16(map + i);
        uint32_t off = rd32(map + i + 2);
        if (id == 0xffff)
            break;
        ResEntry *e = &g_entries[g_count];
        e->type = id >> 11;
        e->number = id & 0x7ff;
        e->volume = off >> shift;
        e->offset = off & ((1u << shift) - 1);
        if (e->volume < 1 || e->volume > nvols)
            continue;
        const ResVolume *v = &g_vols[e->volume - 1];
        uint8_t h[8];
        if (!vol_read(v, e->offset, h, 8))
            continue;
        if (rd16(h) != id)
            continue;
        e->packed_size = rd16(h + 2) - 4;
        e->size = rd16(h + 4);
        e->method = rd16(h + 6);
        if (e->offset + 8 + e->packed_size > v->size)
            continue;
        g_count++;
    }
    detect_methods();
    return g_count;
}

void res_shutdown(void)
{
    for (int i = 0; i < g_count; i++)
        free(g_entries[i].data);
    g_cache_bytes = 0;
    free(g_entries);
    g_entries = NULL;
    g_count = 0;
    g_nvols = 0;
}

int res_count(void) { return g_count; }

ResEntry *res_entry(int index)
{
    return (index >= 0 && index < g_count) ? &g_entries[index] : NULL;
}

ResEntry *res_find(int type, int number)
{
    for (int i = 0; i < g_count; i++)
        if (g_entries[i].type == type && g_entries[i].number == number)
            return &g_entries[i];
    return NULL;
}

const uint8_t *res_load(int type, int number, uint16_t *out_size)
{
    ResEntry *e = res_find(type, number);
    if (!e)
        return NULL;
    if (!e->data) {
        make_room(e->size);
        e->data = decode_entry(e, res_method_algo(e->method));
        if (!e->data)
            return NULL;
        g_cache_bytes += e->size;
    }
    e->stamp = ++g_stamp;
    e->locks++;
    if (out_size)
        *out_size = e->size;
    return e->data;
}

void res_unlock(int type, int number)
{
    ResEntry *e = res_find(type, number);
    if (e && e->locks)
        e->locks--;
}

void res_purge(void)
{
    for (int i = 0; i < g_count; i++) {
        if (g_entries[i].data && !g_entries[i].locks) {
            free(g_entries[i].data);
            g_entries[i].data = NULL;
            g_cache_bytes -= g_entries[i].size;
        }
    }
}
