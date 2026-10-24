#pragma once
/* Sword of Mana (USA/Australia, AVSE) memory map used by the companion; matched against the in-game Status screen. */
#include <stdint.h>
#define SOM_ROM_CODE_ADDR 0x080000ACu /* "AVSE" */
#define ADDR_STATS 0x02025A10u   /* hero: u16 Pow Def Int Mnd Agi, 2 bytes pad; partner follows at +0x0C */
#define STATS_STRIDE 0x0Cu
#define ADDR_VITALS 0x02002E02u  /* hero: u16 HP, maxHP, MP, maxMP; the partner follows at 0x02002E2A */
#define ADDR_VITALS2 0x02002E2Au
#define ADDR_LUCRE 0x02025A70u   /* u32 */
#define ADDR_FRAMES 0x02021B9Cu  /* u32 play time in frames (60/s) */
#define ADDR_HERO_LEVEL 0x02021BD2u /* u16, in the loaded save buffer */
#define ADDR_ITEMS 0x02020D58u   /* 232 x u16 counts: the menus read this copy (a second copy at 0x020030B8 is rewritten from it) */
