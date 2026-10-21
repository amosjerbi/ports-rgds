#include "nds_rom_pager.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    uint32_t page; /* UINT32_MAX = empty */
    uint32_t stamp;
    uint8_t data[NDSROM_PAGE_SIZE];
} Slot;

static FILE* sFile;
static uint32_t sSize, sClock;
static Slot sPool[NDSROM_POOL_PAGES];

int NdsRom_Open(const char* path, NdsRomInfo* info) {
    uint8_t hdr[0xC0];
    NdsRom_Close();
    sFile = fopen(path, "rb");
    if (!sFile)
        return -1;
    fseek(sFile, 0, SEEK_END);
    sSize = (uint32_t)ftell(sFile);
    if (sSize < sizeof hdr || sSize > NDSROM_MAX_SIZE || fseek(sFile, 0, SEEK_SET) ||
        fread(hdr, 1, sizeof hdr, sFile) != sizeof hdr) {
        NdsRom_Close();
        return -2;
    }
    for (uint32_t i = 0; i < NDSROM_POOL_PAGES; i++)
        sPool[i].page = UINT32_MAX;
    if (info) {
        memcpy(info->game_code, hdr + 0xAC, 4);
        info->game_code[4] = 0;
        info->version = hdr[0xBC];
        info->size = sSize;
    }
    return 0;
}

const uint8_t* NdsRom_Map(uint32_t off, uint32_t* avail) {
    if (!sFile || off >= sSize)
        return NULL;
    uint32_t page = off / NDSROM_PAGE_SIZE, victim = 0, oldest = UINT32_MAX;
    for (uint32_t i = 0; i < NDSROM_POOL_PAGES; i++) {
        if (sPool[i].page == page) {
            victim = i;
            goto hit;
        }
        if (sPool[i].page == UINT32_MAX) {
            victim = i;
            oldest = 0;
        } else if (oldest && sPool[i].stamp < oldest) {
            oldest = sPool[i].stamp;
            victim = i;
        }
    }
    if (fseek(sFile, (long)(page * NDSROM_PAGE_SIZE), SEEK_SET))
        return NULL;
    {
        size_t n = fread(sPool[victim].data, 1, NDSROM_PAGE_SIZE, sFile);
        if (n < NDSROM_PAGE_SIZE)
            memset(sPool[victim].data + n, 0, NDSROM_PAGE_SIZE - n);
    }
    sPool[victim].page = page;
hit:
    sPool[victim].stamp = ++sClock;
    if (avail)
        *avail = NDSROM_PAGE_SIZE - (off % NDSROM_PAGE_SIZE);
    return sPool[victim].data + (off % NDSROM_PAGE_SIZE);
}

int NdsRom_Read(uint32_t off, void* dst, uint32_t len) {
    uint8_t* out = dst;
    while (len) {
        uint32_t avail;
        const uint8_t* p = NdsRom_Map(off, &avail);
        if (!p)
            return -1;
        uint32_t n = len < avail ? len : avail;
        memcpy(out, p, n);
        out += n, off += n, len -= n;
    }
    return 0;
}

void NdsRom_Close(void) {
    if (sFile)
        fclose(sFile);
    sFile = NULL;
    sSize = 0;
}
