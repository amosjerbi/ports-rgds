#pragma once
/* Demand-paged GBA ROM access for the NDS (4 MB RAM cannot hold the 16 MB ROM).
 * Pages are 16 KiB, cached LRU in a fixed pool. Pointers returned by
 * NdsRom_Map() are valid only until the next NdsRom_Map() call that evicts
 * their page; callers needing stable data must copy it. */
#include <stddef.h>
#include <stdint.h>

#define NDSROM_PAGE_SIZE  (16u * 1024u)
#define NDSROM_POOL_PAGES 4u /* 64 KiB: probe-only now that the ROM is mapped flat from Slot-2 */
#define NDSROM_MAX_SIZE   (32u * 1024u * 1024u)

typedef struct {
    char game_code[5]; /* "BZME" = USA, "BZMP" = EU */
    uint8_t version;
    uint32_t size;
} NdsRomInfo;

int NdsRom_Open(const char* path, NdsRomInfo* info); /* 0 on success */
const uint8_t* NdsRom_Map(uint32_t rom_offset, uint32_t* avail);
int NdsRom_Read(uint32_t rom_offset, void* dst, uint32_t len);
void NdsRom_Close(void);
