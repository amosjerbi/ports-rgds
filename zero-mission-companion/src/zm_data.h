#pragma once
/* Metroid: Zero Mission (USA/Australia, AGB-BMXE) memory map used by the companion.
 * Everything here was found in the running game (and its save file); nothing from the ROM is shipped. */
#include <stdint.h>

#define ZM_ROM_CODE_ADDR 0x080000ACu /* "BMXE" */

/* ---- IWRAM ------------------------------------------------------------------------------------------------------ */
/* Samus' equipment, 20 bytes (the same bytes sit in the save file):
 *   +0  u16 max energy        +2  u16 max missiles   +4 u8 max super missiles   +5 u8 max power bombs
 *   +6  u16 energy            +8  u16 missiles       +10 u8 super missiles      +11 u8 power bombs
 *   +12 u8 beams + bombs owned   +13 u8 same, switched on
 *   +14 u8 suit / abilities owned +15 u8 same, switched on
 *   +16 u8 downloaded maps (bit per area)
 * Energy shows as tanks of 100 plus the rest (355 = 3 full tanks and 55). */
#define RAM_EQUIP 0x03001530u
#define RAM_EQUIP_SIZE 20
#define EQ_MAX_ENERGY 0
#define EQ_MAX_MISSILES 2
#define EQ_MAX_SUPER 4
#define EQ_MAX_PB 5
#define EQ_ENERGY 6
#define EQ_MISSILES 8
#define EQ_SUPER 10
#define EQ_PB 11
#define EQ_BEAMS 12
#define EQ_BEAMS_ON 13
#define EQ_SUITS 14
#define EQ_SUITS_ON 15
#define EQ_MAPS 16

#define RAM_TIMER 0x03000150u /* u8 hours, minutes, seconds, frames */

/* ---- EWRAM ------------------------------------------------------------------------------------------------------ */
/* Visited map tiles: 7 areas x 32 rows x u32, bit c of row r = tile (c, r) of that area was visited. */
#define RAM_VISITED 0x02037400u
#define AREA_COUNT 7
#define VISITED_AREA_BYTES 128

/* ---- video ------------------------------------------------------------------------------------------------------ */
/* The pause screen (map) draws the whole area as one 64x64-tile layer: BG3, 4bpp, char base 2, screen base 28. */
#define VRAM_BG3_MAP 0x0600E000u
#define VRAM_BG3_CHARS 0x06008000u
#define BG3_CHARS_BYTES 0x6000u

/* beams / bombs byte */
#define BEAM_COUNT 6
static const char* kBeamName[BEAM_COUNT] = { "Long Beam", "Ice Beam", "Wave Beam", "Plasma Beam", "Charge Beam", "Bombs" };
static const uint8_t kBeamBit[BEAM_COUNT] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x80 };
/* suit / ability byte */
#define SUIT_COUNT 8
static const char* kSuitName[SUIT_COUNT] = { "Hi-Jump", "Speed Booster", "Space Jump", "Screw Attack", "Varia Suit", "Gravity Suit", "Morph Ball", "Power Grip" };
static const uint8_t kSuitBit[SUIT_COUNT] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };

static const char* kAreaName[AREA_COUNT] = { "BRINSTAR", "KRAID", "NORFAIR", "RIDLEY", "TOURIAN", "CRATERIA", "CHOZODIA" };
static const char* kAreaShort[AREA_COUNT] = { "BRIN", "KRAID", "NORF", "RIDL", "TOUR", "CRAT", "CHOZ" };
