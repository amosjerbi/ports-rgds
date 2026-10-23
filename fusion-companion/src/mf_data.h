#pragma once
/* Metroid Fusion (USA, AGB-AMTE) memory map used by the companion.
 * Everything here was found in the running game (and its save file); nothing from the ROM is shipped. */
#include <stdint.h>

#define MF_ROM_CODE_ADDR 0x080000ACu /* "AMTE" */

/* ---- IWRAM ------------------------------------------------------------------------------------------------------ */
/* Samus' equipment (the same bytes sit in the save file):
 *   +0 u16 energy   +2 u16 max energy   +4 u16 missiles   +6 u16 max missiles   +8 u8 power bombs   +9 u8 max power bombs
 *   +10 u8 beams owned      bit0 Charge, 1 Wide, 2 Plasma, 3 Wave
 *   +11 u8 missiles / bombs bit0 Missiles, 1 Super, 2 Ice, 3 Diffusion, 4 Power Bombs
 *   +12 u8 suit / abilities bit0 Hi-Jump, 1 Speed Booster, 2 Space Jump, 3 Screw Attack, 4 Varia, 5 Gravity, 6 Morph Ball
 *   +13 u8 security level   +14 u8 downloaded maps (bit per sector)
 * Energy shows as tanks of 100 plus the rest. */
#define RAM_EQUIP 0x03001310u
#define RAM_EQUIP_SIZE 16
#define EQ_ENERGY 0
#define EQ_MAX_ENERGY 2
#define EQ_MISSILES 4
#define EQ_MAX_MISSILES 6
#define EQ_PB 8
#define EQ_MAX_PB 9
#define EQ_BEAMS 10
#define EQ_EXPLOSIVES 11
#define EQ_SUITS 12
#define EQ_SECURITY 13
#define EQ_MAPS 14

#define RAM_TIMER 0x03000138u /* u8 hours, minutes, seconds, frames */

/* ---- EWRAM ------------------------------------------------------------------------------------------------------ */
/* Revealed map tiles: 7 sectors x 32 rows x u32, bit c of row r = tile (c, r) of that sector's map. */
#define RAM_VISITED 0x02037C00u
#define AREA_COUNT 7
#define VISITED_AREA_BYTES 128

/* ---- video ------------------------------------------------------------------------------------------------------ */
/* The pause map draws the whole sector as one 64x64-tile layer: BG3, 4bpp, char base 2, screen base 28 (BG3CNT 0xDC4B). */
#define VRAM_BG3_MAP 0x0600E000u
#define VRAM_BG3_CHARS 0x06008000u
#define BG3_CHARS_BYTES 0x6000u

#define BEAM_COUNT 4
static const char* kBeamName[BEAM_COUNT] = { "Charge Beam", "Wide Beam", "Plasma Beam", "Wave Beam" };
static const uint8_t kBeamBit[BEAM_COUNT] = { 0x01, 0x02, 0x04, 0x08 };
#define MISSILE_COUNT 4
static const char* kMissileName[MISSILE_COUNT] = { "Super Missiles", "Ice Missiles", "Diffusion Missiles", "Power Bombs" };
static const uint8_t kMissileBit[MISSILE_COUNT] = { 0x02, 0x04, 0x08, 0x10 };
#define SUIT_COUNT 7
static const char* kSuitName[SUIT_COUNT] = { "Varia Suit", "Gravity Suit", "Morph Ball", "Hi-Jump", "Speed Booster", "Space Jump", "Screw Attack" };
static const uint8_t kSuitBit[SUIT_COUNT] = { 0x10, 0x20, 0x40, 0x01, 0x02, 0x04, 0x08 };

static const char* kAreaName[AREA_COUNT] = { "MAIN DECK", "SECTOR 1 (SRX)", "SECTOR 2 (TRO)", "SECTOR 3 (PYR)", "SECTOR 4 (AQA)", "SECTOR 5 (ARC)", "SECTOR 6 (NOC)" };
static const char* kAreaShort[AREA_COUNT] = { "DECK", "SRX", "TRO", "PYR", "AQA", "ARC", "NOC" };
