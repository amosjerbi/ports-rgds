#pragma once
/* Castlevania: Harmony of Dissonance (USA, AGB-ACHE) memory map used by the companion.
 * Everything here was derived from the running game and the ROM's own tables; nothing from the ROM is shipped. */
#include <stdint.h>

#define HOD_ROM_CODE_ADDR 0x080000ACu /* "ACHE" */

/* ---- EWRAM (read/written over RetroArch's network command interface) --------------------------------------------- */
#define RAM_BLOCK_BASE 0x02018770u /* one read covers everything from here ... */
#define RAM_BLOCK_SIZE 0x00F8u     /* ... to 0x02018867 */

#define RAM_LEVEL 0x0201877Cu      /* u8 */
#define RAM_SPELLBOOK 0x0201877Fu  /* u8: bit n = spell book n selected, 0x80 flag kept */
#define RAM_EQ_BODY 0x02018780u    /* u8 x4: body, accessory 1..3 = item name index - 97 (0xFF none) */
#define RAM_EQ_ACC1 0x02018781u
#define RAM_EQ_WHIP 0x02018784u    /* u16: whip attachment = item name index - 88 */
#define RAM_HP 0x02018786u         /* u16 */
#define RAM_MP 0x02018788u
#define RAM_HEARTS 0x0201878Au
#define RAM_MAXHEARTS 0x02018794u
#define RAM_EXP 0x02018798u        /* u32 */
#define RAM_GOLD 0x0201879Cu       /* u32 */
#define RAM_CONSUMABLES 0x020187A0u /* u8 counts: item name index 60.. (Potion = 0) */
#define RAM_EQUIPCOUNTS 0x020187BEu /* u8 counts: equipment id = item name index - 97, ids 0..127 */
#define RAM_BOOKS_OWNED 0x0201883Eu /* bit n = spell book n owned */
#define RAM_RELICS_OWNED 0x0201883Fu /* u8 (relics 0..7), next byte low nibble = relics 8..11 */
#define RAM_RELICS_ON 0x02018841u    /* u8 (relics 0..7), next byte low nibble = relics 8..11 */
#define RAM_DEFEATED 0x02018854u     /* bitmap, bit n = enemy n defeated (13 bytes) */

#define RAM_MAXHP 0x0201854Eu   /* u16: computed totals */
#define RAM_MAXMP 0x02018550u
#define RAM_TOTALS 0x02018552u  /* u16 x4: STR DEF INT LCK */
#define RAM_PLAYTIME 0x02000088u /* u32 frames (60/s) */

/* ---- ROM -------------------------------------------------------------------------------------------------------- */
#define ROM_STRING_PTRS 0x08495498u /* u32 pointers, 709 strings; text is u16 codes (see DecodeText) */
#define STR_FIRST 60
#define STR_LAST 617
#define ROM_TEXT_LO 0x080D1700u
#define ROM_TEXT_HI 0x080D9260u
#define STR_ITEM_NAME 60    /* item name index n: names 60..272 */
#define STR_ITEM_DESC 273   /* description of item n is string n + 213 */
#define STR_ENEMY_NAME 486
#define STR_GOLD 611        /* $1 $5 $25 $100 $250 $400 $2000 */

#define ENEMY_COUNT 100
#define ROM_ENEMY_TABLE 0x080C7E44u /* 36-byte records starting at HP: +0 HP u16, +2 EXP u16, +4 level u8, +20 tolerance u8, +21 weakness u8;
                                       the two drops of enemy n are the last 4 bytes of record n-1: (id, type) x2 */
#define ENEMY_REC 36

#define EQUIP_ID_COUNT 128
