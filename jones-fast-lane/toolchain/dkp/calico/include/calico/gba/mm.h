// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#if !defined(__GBA__)
#error "This header file is only for GBA"
#endif

#define MM_BIOS        0x0000000 // 32-bit bus
#define MM_BIOS_SZ        0x4000 // 16kb

#define MM_EWRAM       0x2000000 // 16-bit bus
#define MM_EWRAM_SZ      0x40000 // 256kb

#define MM_IWRAM       0x3000000 // 32-bit bus
#define MM_IWRAM_SZ       0x8000 // 32kb

#define MM_IO          0x4000000 // 32-bit bus
#define MM_IO_SZ           0x400 // 1kb

#define MM_PALRAM      0x5000000 // 16-bit bus
#define MM_PALRAM_SZ       0x400 // 1kb

#define MM_VRAM        0x6000000 // 16-bit bus
#define MM_VRAM_SZ       0x18000 // 96kb

#define MM_OBJRAM      0x7000000 // 32-bit bus
#define MM_OBJRAM_SZ       0x400 // 1kb

#define MM_CARTROM0    0x8000000 // 16-bit bus
#define MM_CARTROM0_SZ 0x2000000 // 32mb

#define MM_CARTROM1    0xa000000 // 16-bit bus
#define MM_CARTROM1_SZ 0x2000000 // 32mb

#define MM_CARTROM2    0xc000000 // 16-bit bus
#define MM_CARTROM2_SZ 0x2000000 // 32mb

#define MM_CARTRAM     0xe000000 // 8-bit bus
#define MM_CARTRAM_SZ    0x10000 // 64kb
