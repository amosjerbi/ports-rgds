#pragma once
/* Final Fantasy Tactics Advance (USA, AGB-AFXE) memory map used by the companion.
 * Everything here was derived from the running game; item names are read from the cartridge's own text at run time
 * (nothing from the ROM is shipped). */
#include <stdint.h>

#define FFTA_ROM_CODE_ADDR 0x080000ACu /* "AFXE" */

/* ---- EWRAM: the live save block -------------------------------------------------------------------------------------- */
#define RAM_UNITS 0x02000080u /* the clan roster: 20 units, 0x108 bytes each */
#define UNIT_COUNT 20
#define UNIT_STRIDE 0x108u
#define U_NAME_PTR 0x00u  /* u32: custom names point into EWRAM (0x80 + letter pairs), the others into the ROM's name list */
#define U_LEVEL 0x09u     /* u8 */
#define U_HP 0x18u        /* u16 current, u16 max */
#define U_MP 0x1Cu        /* u16 current, u16 max */
#define U_STATS 0x20u     /* 4 x u16 base Attack, Defense, Magic Power, Magic Resistance (before equipment) */
#define U_EQUIP 0x2Au     /* 5 x u16 item ids: weapon, helm, armor, boots, accessory (0 = none) */
#define U_JOB 0x35u       /* u8 job id */
#define U_SPEED 0xD2u     /* u16 */
#define EQUIP_SLOTS 5

#define RAM_GIL 0x02001F64u      /* u32 clan funds */
#define RAM_CLAN_NAME 0x02000052u /* the clan's name (0x80 + letter pairs) */
#define RAM_CLAN_LEVEL 0x020021B4u /* u8 clan level, followed by u8 clan points */

/* shared inventory: 4-byte entries { u16 item id, u8 owned, u8 equipped } */
#define RAM_INV 0x02001940u
#define INV_BYTES 0x5D8u
#define INV_ENTRIES (INV_BYTES / 4)

/* ---- ROM: name pool and pointer table (item name = table index id + 0x7B) ---------------------------------------------- */
#define ROM_POOL 0x08523200u
#define ROM_POOL_BYTES 0x3480u
#define ROM_NAME_TABLE 0x08526680u
#define ITEM_INDEX_BASE 0x7Bu
#define ITEM_ID_LAST 0x1CBu
#define ITEM_ID_ARMOR 0x109u  /* ids 1..0x108 weapons, 0x109..0x169 armor and accessories, 0x16A.. consumables and loot */
#define ITEM_ID_USE 0x16Au
#define ITEM_ID_BODY 0x120u   /* helms 0x109..0x11F, body armor and robes 0x120..0x152, boots 0x153..0x15C, accessories 0x15D.. */
#define ITEM_ID_FEET 0x153u
#define ITEM_ID_ACC 0x15Du
