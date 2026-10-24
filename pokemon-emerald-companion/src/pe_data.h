#pragma once
/* Pokemon Emerald (USA/Europe, BPEE) memory map used by the companion. Layouts follow the public pokeemerald decomp;
 * addresses were checked against the running game (party, money, badges, Pokedex, bag). */
#include <stdint.h>
#define PE_ROM_CODE_ADDR 0x080000ACu /* "BPEE" */

#define ADDR_SB1_PTR 0x03005D8Cu /* gSaveBlock1Ptr, gSaveBlock2Ptr, gPokemonStoragePtr: the blocks move every time the game loads */
#define ADDR_SB2_PTR 0x03005D90u
#define ADDR_PARTY_COUNT 0x020244E9u
#define ADDR_PARTY 0x020244ECu /* 6 x 100 bytes */
#define MON_SIZE 100

/* SaveBlock2 */
#define SB2_NAME 0x00u    /* 8 bytes, game text */
#define SB2_GENDER 0x08u
#define SB2_TID 0x0Au     /* u16 trainer id, u16 secret id */
#define SB2_PLAY 0x0Eu    /* u16 hours, u8 minutes, u8 seconds */
#define SB2_DEX_OWNED 0x28u /* 52-byte bitfield */
#define SB2_DEX_SEEN 0x5Cu
#define SB2_KEY 0xACu     /* u32 encryption key for money, coins and bag quantities */
#define SB2_READ 0xB0u

/* SaveBlock1: one read from SB1_LO covers money and every bag pocket */
#define SB1_LO 0x490u
#define SB1_LO_LEN 0x3C0u
#define SB1_MONEY 0x490u  /* u32 ^ key */
#define SB1_COINS 0x494u  /* u16 ^ key */
#define SB1_BADGES 0x137Cu /* flag byte holding FLAG_BADGE01..08 (flag 0x867..0x86E) */
#define SB1_BAG_ITEMS 0x560u /* 30 x (u16 id, u16 qty ^ key) */
#define SB1_BAG_KEY 0x5D8u   /* 30 */
#define SB1_BAG_BALLS 0x650u /* 16 */
#define SB1_BAG_TMS 0x690u   /* 64 */
#define SB1_BAG_BERRIES 0x790u /* 46 */

/* ROM text tables (game text, 0xFF terminated) */
#define ROM_SPECIES 0x083185C8u /* 11 bytes each, indexed by the internal species number */
#define SPECIES_COUNT 412
#define ROM_MOVES 0x0831977Cu   /* 13 bytes each */
#define MOVE_COUNT 355
#define ROM_ITEMS 0x085839A0u   /* 44-byte records, name first */
#define ITEM_REC 44
#define ITEM_COUNT 377
