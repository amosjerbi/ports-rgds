#pragma once
/* Shining Soul II (USA, AU2E) memory map used by the companion; matched against the game's inventory screen. */
#include <stdint.h>
#define SS2_ROM_CODE_ADDR 0x080000ACu /* "AU2E" */
#define ADDR_NAME 0x02003872u    /* 7 x u16 character codes, 'A' = 0x21 */
#define NAME_LEN 7
#define ADDR_STATS 0x02003884u   /* u16 STR DEX INT VIT */
#define ADDR_GOLD 0x0200388Cu    /* u16 */
#define ADDR_ATKDEF 0x020038A4u  /* u16 ATK, u16 DEF */
#define ADDR_HP_LIVE 0x03003ED0u /* u32 current HP: the game's authoritative copy (the EWRAM mirror follows it) */
#define ADDR_VITALS 0x0200B260u  /* u32 hp, sp x10, 0, max hp, max sp x10 */
