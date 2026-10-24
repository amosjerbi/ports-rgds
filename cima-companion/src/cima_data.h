#pragma once
/* CIMA: The Enemy (USA, BCME) memory map used by the companion. Found by matching the party screen against RAM. */
#include <stdint.h>
#define CIMA_ROM_CODE_ADDR 0x080000ACu /* "BCME" */
#define UNIT_BASE 0x02020224u /* 16 character records, 0xDC bytes each (the game keeps a second copy at 0x02012A28) */
#define UNIT_STRIDE 0xDCu
#define UNIT_COUNT 16
#define U_HP 0x30u    /* u8 current HP, then u8 max HP; the party screen shows these */
#define U_STAT 0x01u  /* 4 x u8 stats (same four numbers as the party screen grid) */
#define ROM_NAMES 0x080C3340u /* not used; names are compiled in below, in the game's roster order */
