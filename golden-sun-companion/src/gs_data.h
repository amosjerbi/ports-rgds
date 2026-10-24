#pragma once
/* Golden Sun (USA/Europe, AGB-AGSE) memory map used by the companion.
 * Everything here was found in the running game (and its save file); nothing from the ROM is shipped. */
#include <stdint.h>

#define GS_ROM_CODE_ADDR 0x080000ACu /* "AGSE" */

/* ---- EWRAM ------------------------------------------------------------------------------------------------------ */
#define RAM_COINS 0x02000250u /* u32 */

/* The four party members, 0x14C bytes each (the same bytes sit in the save file):
 *   +0x00 name (ASCII, 15 bytes)   +0x0F u8 level
 *   +0x34 u16 max HP   +0x36 u16 max PP   +0x38 u16 HP   +0x3A u16 PP
 *   +0x3C u16 attack   +0x3E u16 defense   +0x40 u16 agility   +0x42 u8 luck
 *   +0x48 u16 x8: power / resist of Venus, Mercury, Mars, Jupiter (interleaved)
 *   +0xF8 u32 x4: djinn owned, one bit per djinn (Venus, Mercury, Mars, Jupiter)
 *   +0x108 u32 x4: djinn set (not on standby)
 *   +0x118 u8 x4: djinn set per element   +0x11C u8 x4: djinn owned per element
 *   +0x124 u32 experience */
#define RAM_PARTY 0x02000500u
#define PARTY_STRIDE 0x14C
#define PARTY_COUNT 4
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

#define ELEMENT_COUNT 4
static const char* kElementName[ELEMENT_COUNT] = { "VENUS", "MERCURY", "MARS", "JUPITER" };
#define DJINN_PER_ELEMENT 7
static const char* kDjinnName[ELEMENT_COUNT][DJINN_PER_ELEMENT] = {
    { "Flint", "Granite", "Quartz", "Vine", "Sap", "Ground", "Bane" },
    { "Fizz", "Sleet", "Mist", "Spritz", "Hail", "Tonic", "Dew" },
    { "Forge", "Fever", "Corona", "Scorch", "Ember", "Flash", "Torch" },
    { "Gust", "Breeze", "Zephyr", "Smog", "Kite", "Squall", "Luff" },
};
