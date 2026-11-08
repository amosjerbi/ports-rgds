/* SCI resource manager (SCI0/SCI1-early map format).
 * Portable C: used both by the macOS test tools and the NDS build. */
#ifndef SCI_RESOURCE_H
#define SCI_RESOURCE_H

#include <stdint.h>
#include <stddef.h>

enum {
    RES_VIEW = 0, RES_PIC, RES_SCRIPT, RES_TEXT, RES_SOUND, RES_MEMORY,
    RES_VOCAB, RES_FONT, RES_CURSOR, RES_PATCH, RES_BITMAP, RES_PALETTE,
    RES_CDAUDIO, RES_AUDIO, RES_SYNC,
    RES_TYPE_COUNT = 32
};

typedef struct {
    uint8_t  type;
    uint16_t number;
    uint8_t  volume;
    uint32_t offset;
    uint16_t packed_size;   /* bytes of compressed payload */
    uint16_t size;          /* decompressed size */
    uint16_t method;
    uint8_t *data;          /* cached decompressed data, or NULL */
    uint16_t locks;
    uint32_t stamp;         /* LRU */
} ResEntry;

/* A volume is either in memory (`data`) or read on demand (`read`, when
 * data is NULL; returns nonzero on success). */
typedef struct {
    const uint8_t *data;
    uint32_t       size;
    int          (*read)(void *user, uint32_t offset, void *buf, uint32_t n);
    void          *user;
} ResVolume;

#define RES_MAX_VOLUMES 8

/* Compression method ids as stored in the volume header. Which algorithm each
 * maps to depends on interpreter version; res_init() detects it. */
typedef enum { COMP_NONE, COMP_LZW, COMP_HUFFMAN, COMP_LZW1 } CompAlgo;

int  res_init(const uint8_t *map, uint32_t map_size,
              const ResVolume *vols, int nvols);  /* vols[i] = resource.00(i+1) */
void res_shutdown(void);

int        res_count(void);
ResEntry  *res_entry(int index);
ResEntry  *res_find(int type, int number);

/* Returns decompressed data (cached, owned by the manager) or NULL. */
const uint8_t *res_load(int type, int number, uint16_t *out_size);
void           res_unlock(int type, int number);
void           res_purge(void);           /* free all unlocked cached data */
void           res_set_cache_budget(uint32_t bytes);  /* 0 = unlimited */
uint32_t       res_cache_bytes(void);

const char *res_type_name(int type);
CompAlgo    res_method_algo(int method);
int         res_decompress(CompAlgo algo, const uint8_t *src, uint32_t src_len,
                           uint8_t *dst, uint32_t dst_len);

#endif
