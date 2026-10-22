#pragma once
/* Castlevania: Aria of Sorrow (USA, AGB-A2CE) memory map used by the companion.
 * Everything here was derived from the running game and the ROM's own code/data; nothing from the ROM is shipped.
 *
 * RAM (EWRAM, read/written over RetroArch's network command interface):
 *   save block ("slot") at 0x0201324C, mirrors the SRAM slot byte for byte.
 */
#include <stdint.h>

#define AOS_ROM_CODE_ADDR 0x080000ACu /* "A2CE" */

/* ---- RAM -------------------------------------------------------------------------------------------------------- */
#define RAM_WINDOW_BASE 0x020131E0u /* one read covers everything below */
#define RAM_WINDOW_SIZE 0x0234u

#define RAM_ATT 0x020131EEu      /* u16, computed totals incl. equipment */
#define RAM_DEF 0x020131F0u      /* u16 */
#define RAM_STR_TOTAL 0x020131F2u /* u16 x4: STR CON INT LCK */

#define RAM_EQ_WEAPON 0x02013268u  /* u8: item index - 32 (0xFF none) */
#define RAM_EQ_BULLET 0x02013269u  /* u8: soul index within type + 1 (0 none) */
#define RAM_EQ_GUARDIAN 0x0201326Au
#define RAM_EQ_ENCHANT 0x0201326Bu
#define RAM_EQ_ARMOR 0x0201326Cu   /* u8: item index - 91 */
#define RAM_EQ_ACCESSORY 0x0201326Du /* u8: item index - 91 */

#define RAM_LEVEL 0x02013279u /* u8 */
#define RAM_HP 0x0201327Au    /* u16 */
#define RAM_MP 0x0201327Cu
#define RAM_MAXHP 0x0201327Eu
#define RAM_MAXMP 0x02013280u
#define RAM_BASE_STATS 0x02013282u /* u16 x4: STR CON INT LCK */
#define RAM_EXP 0x0201328Cu   /* u32 */
#define RAM_GOLD 0x02013290u  /* u32 */
#define RAM_ITEMS 0x02013294u /* u8 x136: count per item index (0..9) */

#define RAM_SOULS0 0x0201331Cu /* nibble arrays, low nibble first: bullet(56) */
#define RAM_SOULS1 0x02013354u /* guardian(24) */
#define RAM_SOULS2 0x0201336Eu /* enchant(35) */
#define RAM_SOULS3 0x02013392u /* ability(6) */
#define RAM_ABILITY_MASK 0x02013396u /* u8: bit n = ability soul n switched on */
#define RAM_PLAYTIME 0x020000ACu /* u32 frames (60/s), live counter (the save block copy only updates when saving) */

#define ITEM_COUNT 136
#define SOUL_COUNT 121

/* ---- ROM -------------------------------------------------------------------------------------------------------- */
#define ROM_STRING_PTRS 0x08506B38u /* u32 pointers; text is "01 00 <chars> 06 0A", 0x06 = line break */
#define STR_ITEM_NAME 91
#define STR_ITEM_DESC 348
#define STR_SOUL_NAME 227
#define STR_SOUL_DESC 484
#define STR_ENEMY_NAME 605
#define STR_ENEMY_DESC 718
#define STR_FIRST 91
#define STR_LAST 830
#define ROM_TEXT_LO 0x080F2B08u /* lowest/highest text address of strings 91..830 */
#define ROM_TEXT_HI 0x080F8390u

#define ENEMY_COUNT 113
#define ROM_ENEMY_TABLE 0x080E964Cu /* 36-byte records */
#define ENEMY_REC 36
/* record: +0 u16 drop1+1, +2 u16 drop2+1 (item index + 1), +4 u16 HP, +8 u16 EXP, +15 u8 soul type,
 * +16 u16 soul index in type +1, +18 u16 weakness bits, +20 u16 tolerance bits */

/* soul types: 0 bullet(red) 1 guardian(blue) 2 enchant(yellow) 3 ability(white) */
static const int kSoulTypeFirst[4] = { 0, 56, 80, 115 };
static const int kSoulTypeCount[4] = { 56, 24, 35, 6 };
static const uint32_t kSoulRam[4] = { RAM_SOULS0, RAM_SOULS1, RAM_SOULS2, RAM_SOULS3 };

/* item index ranges */
#define ITEM_CONSUMABLE_FIRST 0
#define ITEM_CONSUMABLE_LAST 31
#define ITEM_WEAPON_FIRST 32
#define ITEM_WEAPON_LAST 90
#define ITEM_ARMOR_FIRST 91
#define ITEM_ARMOR_LAST 118
#define ITEM_ACCESSORY_FIRST 119
#define ITEM_ACCESSORY_LAST 135
