#pragma once
/* Castlevania: Circle of the Moon (USA, AGB-AAME) memory map used by the companion.
 * Everything here was derived from the running game and the ROM's own tables; nothing from the ROM is shipped. */
#include <stdint.h>

#define COTM_ROM_CODE_ADDR 0x080000ACu /* "AAME" */

/* ---- EWRAM: the live save slot -------------------------------------------------------------------------------------
 * The game keeps its save data at 0x02025358 (byte for byte what is written to SRAM: "DRACULA AGB" header first) and
 * works on it directly, so one read of SAVE_SIZE bytes covers everything the companion shows. Offsets below are
 * relative to SAVE_BASE. */
#define SAVE_BASE 0x02025358u
#define SAVE_SIZE 0x0400u

#define SV_PLAYTIME 0x0A0u   /* u32 frames (60/s) */
#define SV_MAP 0x100u        /* 34 rows x 8 bytes; the low 48 bits of each row are one explored-room flag per map column, LSB = column 0 */
#define MAP_COLS 48
#define MAP_ROWS 34
#define SV_HP 0x2D6u         /* u16; the copy at +4 is the other one of (current, max) */
#define SV_HP2 0x2DAu
#define SV_MP 0x2DEu
#define SV_MP2 0x2E2u
#define SV_HEARTS 0x2E4u     /* u16 current, u16 max */
#define SV_STR 0x2EEu        /* u16 totals; the game recalculates them (3 copies each) whenever the equipment bytes change */
#define SV_DEF 0x2F6u
#define SV_INT 0x2FEu
#define SV_LCK 0x306u
#define SV_LEVEL 0x30Cu      /* u8 */
#define SV_EXP 0x310u        /* u32 */
#define SV_DSS_ATTR 0x314u   /* u8: equipped attribute card, 1 + index (0 = none) */
#define SV_DSS_ACT 0x315u    /* u8: equipped action card, 1 + index (0 = none) */
#define SV_EQ_BODY 0x316u    /* u8 item id (0 = none) */
#define SV_EQ_ARM_R 0x317u
#define SV_EQ_ARM_L 0x318u
#define SV_CARDS 0x31Cu      /* 20 x u8 owned flag: 10 action cards (Mercury..Pluto), 10 attribute cards (Salamander..Black Dog) */
#define SV_COMBOS 0x330u     /* 100 x u8 "combination found" flag, index = action * 10 + attribute */
#define SV_COUNTS 0x394u     /* u8 stock count per item id 0..54 (a worn item is not in the stock count) */
#define SV_MAGIC 0x3D7u      /* 8 x u8 owned flag: Dash Boots, Double, Tackle, Kick Boots, Heavy Ring, Cleansing, Roc Wing, Last Key */

#define ITEM_LAST 54
#define ITEM_ARMOR_LAST 22   /* ids 1..22 body armor, 23..40 arm accessories, 41..54 consumables */
#define ITEM_ARM_FIRST 23
#define ITEM_ARM_LAST 40
#define ITEM_USE_FIRST 41

/* ---- ROM ---------------------------------------------------------------------------------------------------------- */
#define ROM_TEXT_LO 0x08391A00u /* one read covers all the tables below */
#define ROM_TEXT_HI 0x083965C0u
#define ROM_NAMES 0x08391A0Fu   /* "Unequip", two filler strings, then the item names for ids 1..54 */
#define ROM_NAME_COUNT 57
#define ROM_DESCS 0x08395583u   /* 55 item descriptions, id 0..54 */
#define ROM_COMBOS 0x083940D5u  /* 101 DSS combination descriptions: [0] "No effect", then 1 + action * 10 + attribute */
#define ROM_CARDS 0x08395EDAu   /* 20 card descriptions: 10 attribute cards, then 10 action cards */
#define ROM_MAGIC 0x08393EAAu   /* 8 magic item descriptions */

#define ROM_EQUIP_STATS 0x080E0970u /* 4 x s16 (STR DEF INT LCK) per item id */
