#pragma once
/* Mother 3 (fan translation v1.3, A3UJ) memory map used by the companion; matched against the game's Status and Goods screens. */
#include <stdint.h>
#define M3_ROM_CODE_ADDR 0x080000ACu /* "A3UJ" */

/* Per-character save records, indexed by character id (0 Lucas, 1 Duster, 2 Kumatora, 3 Boney, ...), stride 0x6C.
 * Fields are relative to the record's HP word at CHAR_BASE + id * CHAR_STRIDE. */
#define CHAR_BASE 0x02004200u
#define CHAR_STRIDE 0x6Cu
#define CHAR_COUNT 8
#define CH_MAXHP 0x08u   /* u32 */
#define CH_EQUIP 0x1Cu   /* 4 x u8 item id: weapon, body, head, other */
#define CH_ITEMS 0x24u   /* 16 x u8 item id, the Goods list read row by row (left, right, left, right ...) */
#define CH_NAME (-0x16)  /* u16 codes: 'A' = 0x21, 'a' = 0x41, space = 0x40, ended by 0xFFFF */
#define CH_LEVEL (-6)    /* u8 */
#define CH_EXP (-4)      /* u32 */
#define NAME_LEN 8

#define ADDR_DP 0x02004868u /* u32 money */

/* Party slots (the order of the HUD and of the Goods tabs): live HP / PP are the middle word of a 3-word group */
#define PARTY_SLOTS 4
#define ADDR_HP_LIVE 0x0200DE10u /* u16, + 0x0C per slot */
#define ADDR_PP_LIVE 0x0200DE4Cu /* u16, + 0x0C per slot */
#define SLOT_STATS 0x0200D818u   /* 0x30 per slot: u16 HP at +0, max HP +8, max PP +0x10, offense +0x16, defense +0x1C, IQ +0x22, speed +0x28 */
#define SLOT_STRIDE 0x30u

/* menu state: 0 on the field, 14 main menu, 10 Goods list / its Use and target prompts */
#define ADDR_MENU 0x02005C00u
