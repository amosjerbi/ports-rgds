// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once

/*! @addtogroup cache
	@{
*/

//! @brief Instruction cache size on the DS/3DS's Arm9 CPU
#define ARM_ICACHE_SZ 0x2000

//! @brief Log2(data cache size) on the DS/3DS's Arm9 CPU
#define ARM_DCACHE_LOG2   12

//! @brief Data cache size on the DS/3DS Arm9 CPU's data cahce
#define ARM_DCACHE_SZ 0x1000

//! @brief Log2(cache line size) on the DS/3DS's Arm9 CPU's data cache
#define ARM_CACHE_LINE_LOG2 5

//! @brief Cache line size on the DS/3DS's Arm9 CPU's data cache
#define ARM_CACHE_LINE_SZ 32

//! @brief Log2(number of ways) on the on the DS/3DS Arm9 CPU's data cache
#define ARM_DCACHE_WAYS_LOG2 2

//! @brief Number of ways on the on the DS/3DS's Arm9 CPU
#define ARM_DCACHE_WAYS 4

//! @brief Number of index bits (= log2(number of sets)) on the DS/3DS's Arm9 CPU
#define ARM_DCACHE_NUM_INDEX_BITS (ARM_DCACHE_LOG2 - ARM_DCACHE_WAYS_LOG2 - ARM_CACHE_LINE_LOG2)

//! @brief Number of tag bits on the DS/3DS's Arm9 CPU
#define ARM_DCACHE_NUM_TAG_BITS (32 - ARM_DCACHE_NUM_INDEX_BITS - ARM_CACHE_LINE_LOG2)

//! @}

/*! @addtogroup cp15
	@{
*/
/*! @name CP15 control register
	@{
*/

#define CP15_CR_PU_ENABLE     (1<<0)    //!< Enable the protection unit
#define CP15_CR_DCACHE_ENABLE (1<<2)    //!< Enable data cache
#define CP15_CR_SB1           (0xF<<3)  //!< Reserved bits, should be one
#define CP15_CR_BIG_ENDIAN    (1<<7)    //!< Use big-endian byte order for all memory fetches
#define CP15_CR_ICACHE_ENABLE (1<<12)   //!< Enable instruction cache
#define CP15_CR_ALT_VECTORS   (1<<13)   //!< If set, exceptions vectors are located at 0xFFFF0000, otherwise at 0x00000000
#define CP15_CR_ROUND_ROBIN   (1<<14)   //!< If set, use a round-robin cache replacement algo, otherwise it is pseudo-random
#define CP15_CR_DISABLE_TBIT  (1<<15)   //!< If set, prevents instruction that load PC from changing THUMB state
#define CP15_CR_DTCM_ENABLE   (1<<16)   //!< Enable data TCM
#define CP15_CR_DTCM_LOAD     (1<<17)   //!< Put the data TCM in load mode
#define CP15_CR_ITCM_ENABLE   (1<<18)   //!< Enable instruction TCM
#define CP15_CR_ITCM_LOAD     (1<<19)   //!< Put the instruction TCM in load mode

//! @}

/*! @name ITCM/DTCM size register
	@{
*/

#define CP15_TCM_4K   (0b00011 << 1)    //!< 4 KiB TCM size
#define CP15_TCM_8K   (0b00100 << 1)    //!< 8 KiB TCM size
#define CP15_TCM_16K  (0b00101 << 1)    //!< 16 KiB TCM size
#define CP15_TCM_32K  (0b00110 << 1)    //!< 32 KiB TCM size
#define CP15_TCM_64K  (0b00111 << 1)    //!< 64 KiB TCM size
#define CP15_TCM_128K (0b01000 << 1)    //!< 128 KiB TCM size
#define CP15_TCM_256K (0b01001 << 1)    //!< 256 KiB TCM size
#define CP15_TCM_512K (0b01010 << 1)    //!< 512 KiB TCM size
#define CP15_TCM_1M   (0b01011 << 1)    //!< 1 MiB TCM size
#define CP15_TCM_2M   (0b01100 << 1)    //!< 2 MiB TCM size
#define CP15_TCM_4M   (0b01101 << 1)    //!< 4 MiB TCM size
#define CP15_TCM_8M   (0b01110 << 1)    //!< 8 MiB TCM size
#define CP15_TCM_16M  (0b01111 << 1)    //!< 16 MiB TCM size
#define CP15_TCM_32M  (0b10000 << 1)    //!< 32 MiB TCM size
#define CP15_TCM_64M  (0b10001 << 1)    //!< 64 MiB TCM size
#define CP15_TCM_128M (0b10010 << 1)    //!< 128 MiB TCM size
#define CP15_TCM_256M (0b10011 << 1)    //!< 256 MiB TCM size

//! @}

/*! @name MPU region configuration register
	@{
*/

#define CP15_PU_ENABLE 1                //!< Enable memory region entry

#define CP15_PU_4K   (0b01011 << 1)     //!< 4 KiB memory region
#define CP15_PU_8K   (0b01100 << 1)     //!< 8 KiB memory region
#define CP15_PU_16K  (0b01101 << 1)     //!< 16 KiB memory region
#define CP15_PU_32K  (0b01110 << 1)     //!< 32 KiB memory region
#define CP15_PU_64K  (0b01111 << 1)     //!< 64 KiB memory region
#define CP15_PU_128K (0b10000 << 1)     //!< 128 KiB memory region
#define CP15_PU_256K (0b10001 << 1)     //!< 256 KiB memory region
#define CP15_PU_512K (0b10010 << 1)     //!< 512 KiB memory region
#define CP15_PU_1M   (0b10011 << 1)     //!< 1 MiB memory region
#define CP15_PU_2M   (0b10100 << 1)     //!< 2 MiB memory region
#define CP15_PU_4M   (0b10101 << 1)     //!< 4 MiB memory region
#define CP15_PU_8M   (0b10110 << 1)     //!< 8 MiB memory region
#define CP15_PU_16M  (0b10111 << 1)     //!< 16 MiB memory region
#define CP15_PU_32M  (0b11000 << 1)     //!< 32 MiB memory region
#define CP15_PU_64M  (0b11001 << 1)     //!< 64 MiB memory region
#define CP15_PU_128M (0b11010 << 1)     //!< 128 MiB memory region
#define CP15_PU_256M (0b11011 << 1)     //!< 256 MiB memory region
#define CP15_PU_512M (0b11100 << 1)     //!< 512 MiB memory region
#define CP15_PU_1G   (0b11101 << 1)     //!< 1 GiB memory region
#define CP15_PU_2G   (0b11110 << 1)     //!< 2 GiB memory region
#define CP15_PU_4G   (0b11111 << 1)     //!< 4 GiB memory region

//! @}

/*! @name MPU region permissions
	@{
*/

#define CP15_PU_PERM_NONE    0  //!< PRIV=-- USR=-- memory region permissions
#define CP15_PU_PERM_PRIV_RW 1  //!< PRIV=RW USR=-- memory region permissions
#define CP15_PU_PERM_RW_R    2  //!< PRIV=RW USR=R- memory region permissions
#define CP15_PU_PERM_RW      3  //!< PRIV=RW USR=RW memory region permissions
#define CP15_PU_PERM_PRIV_RO 5  //!< PRIV=R- USR=-- memory region permissions
#define CP15_PU_PERM_RO      6  //!< PRIV=R- USR=R- memory region permissions

//! @}

//! @}
