#pragma once
/* Golden Sun: The Lost Age (USA/Europe, AGB-AGFE) memory map used by the companion.
 * Everything here was found in the running game; nothing from the ROM is shipped. */
#include <stdint.h>

#define GSL_ROM_CODE_ADDR 0x080000ACu /* "AGFE" */

/* ---- EWRAM ------------------------------------------------------------------------------------------------------ */
#define RAM_COINS 0x02000250u /* u32 */
#define RAM_PARTY_LIST 0x0200001Cu /* u8 x4: character slots in the active party, 0xFF ends the list (the save header's copy) */

/* The eight character slots, 0x14C bytes each (same layout as the first Golden Sun, which is where slots 0..3 come from):
 *   +0x00 name (the game's own character codes)   +0x0F u8 level
 *   +0x34 u16 max HP   +0x36 u16 max PP   +0x38 u16 HP   +0x3A u16 PP
 *   +0x3C u16 attack   +0x3E u16 defense   +0x40 u16 agility   +0x42 u8 luck
 *   +0x48 u16 x8: power / resist of Venus, Mercury, Mars, Jupiter (interleaved)
 *   +0xF8 u32 x4: djinn held, one bit per djinn (Venus, Mercury, Mars, Jupiter; 18 bits each)
 *   +0x108 u32 x4: djinn set (not on standby)
 *   +0x118 u8 x4: djinn set per element   +0x11C u8 x4: djinn held per element
 *   +0x124 u32 experience */
#define RAM_PARTY 0x02000520u
#define PARTY_STRIDE 0x14C
#define PARTY_COUNT 8
#define CH_NAME 0x00
#define CH_LEVEL 0x0F
#define CH_MAX_HP 0x34
#define CH_MAX_PP 0x36
#define CH_HP 0x38
#define CH_PP 0x3A
#define CH_ATTACK 0x3C
#define CH_DEFENSE 0x3E
#define CH_AGILITY 0x40
#define CH_LUCK 0x42
#define CH_ELEMENTS 0x48
#define CH_DJINN_OWNED 0xF8
#define CH_DJINN_SET 0x108
#define CH_DJINN_SET_COUNT 0x118
#define CH_DJINN_OWNED_COUNT 0x11C
#define CH_EXP 0x124

/* The slot order is fixed by the game (the names are stored as kana in some saves, so the English names come from here). */
static const char* kCharName[PARTY_COUNT] = { "Isaac", "Garet", "Ivan", "Mia", "Felix", "Jenna", "Sheba", "Piers" };

#define ELEMENT_COUNT 4
static const char* kElementName[ELEMENT_COUNT] = { "VENUS", "MERCURY", "MARS", "JUPITER" };
#define DJINN_PER_ELEMENT 18
/* Bit order of the Djinn masks: the 7 of the first game, then the 11 new ones. "Mars #n" = name not confirmed yet. */
static const char* kDjinnName[ELEMENT_COUNT][DJINN_PER_ELEMENT] = {
    { "Flint", "Granite", "Quartz", "Vine", "Sap", "Ground", "Bane", "Echo", "Iron", "Steel", "Mud", "Flower", "Meld", "Petra", "Salt", "Geode", "Mold", "Crystal" },
    { "Fizz", "Sleet", "Mist", "Spritz", "Hail", "Tonic", "Dew", "Fog", "Sour", "Spring", "Shade", "Chill", "Steam", "Rime", "Gel", "Eddy", "Balm", "Serac" },
    { "Forge", "Fever", "Corona", "Scorch", "Ember", "Flash", "Torch", "Cannon", "Spark", "Kindle", "Char", "Coal", "Reflux", "Core", "Tinder", "Mars #16", "Mars #17", "Mars #18" },
    { "Gust", "Breeze", "Zephyr", "Smog", "Kite", "Squall", "Luff", "Breath", "Blitz", "Ether", "Waft", "Haze", "Wheeze", "Aroma", "Whorl", "Gasp", "Lull", "Gale" },
};
