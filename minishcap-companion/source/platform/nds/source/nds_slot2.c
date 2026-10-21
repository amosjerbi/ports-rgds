#include "nds_slot2.h"

#include <nds.h>
#include <string.h>

int Nds_AttachSlot2Rom(const uint8_t** base, uint32_t* size, char game_code[5]) {
    static const uint8_t kLogoHead[4] = { 0x24, 0xFF, 0xAE, 0x51 }; /* GBA header logo */
    const volatile uint8_t* rom = (const volatile uint8_t*)NDS_SLOT2_BASE;
    uint8_t hdr[0xB0];

    if (isDSiMode())
        return -1; /* no Slot-2 on DSi */
    sysSetCartOwner(BUS_OWNER_ARM9);
    for (unsigned i = 0; i < sizeof hdr; i++)
        hdr[i] = rom[i];
    if (memcmp(hdr + 4, kLogoHead, 4))
        return -2; /* nothing GBA-shaped in the slot */
    memcpy(game_code, hdr + 0xAC, 4);
    game_code[4] = 0;
    if (strcmp(game_code, "BZME") && strcmp(game_code, "BZMP"))
        return -3; /* not a supported Minish Cap ROM */
    if (base)
        *base = (const uint8_t*)NDS_SLOT2_BASE;
    if (size)
        *size = NDS_SLOT2_ROM_SIZE;
    return 0;
}
