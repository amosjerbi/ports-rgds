#pragma once
/* GBA-slot (Slot-2) ROM access. On DS/DS Lite the cart bus maps the GBA ROM at
 * 0x08000000, the same address the engine already uses for ROM pointers. Pointing
 * gRomData at it keeps every native pointer into the ROM valid (a paged cache
 * cannot: ~90 call sites hold raw ROM pointers). Needs the original cartridge or
 * a GBA flashcart (e.g. EZ-Flash Omega DS) holding the ROM. Not available on DSi/3DS. */
#include <stdint.h>

#define NDS_SLOT2_BASE 0x08000000u
#define NDS_SLOT2_ROM_SIZE (16u * 1024u * 1024u)

/* Returns 0 and fills base/size/game_code if a Minish Cap ROM is present. */
int Nds_AttachSlot2Rom(const uint8_t** base, uint32_t* size, char game_code[5]);
