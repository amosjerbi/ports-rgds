// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#if !defined(__NDS__)
#error "This header file is only for NDS"
#endif

/*! @addtogroup mm
	@{
*/

#ifdef ARM7
#define MM_BIOS        0x0000000 // 32-bit bus
#define MM_BIOS_SZ_NTR    0x4000 // 16kb
#define MM_BIOS_SZ_TWL   0x10000 // 64kb
#endif

#ifdef ARM9
#define MM_VECTORS     0x0000000 // ITCM
#define MM_VECTORS_SZ       0x20 // 32 bytes

#define MM_ITCM        0x1ff8000 // ITCM
#define MM_ITCM_SZ        0x8000 // 32kb
#endif

#define MM_MAINRAM        0x2000000 // 16-bit bus
#define MM_MAINRAM_SZ_NTR  0x400000 // 4mb
#define MM_MAINRAM_SZ_DBG  0x800000 // 8mb
#define MM_MAINRAM_SZ_TWL 0x1000000 // 16mb

#ifdef ARM9
#define MM_DTCM        0x2ff0000 // DTCM
#define MM_DTCM_SZ     0x4000    // 16kb
#endif

#define MM_TWLWRAM_MAP       0x3000000
#define MM_TWLWRAM_BANK_SZ     0x40000 // 256kb
#define MM_TWLWRAM_SLOT_A_SZ   0x10000 // 64kb
#define MM_TWLWRAM_SLOT_B_SZ    0x8000 // 32kb
#define MM_TWLWRAM_SLOT_C_SZ    0x8000 // 32kb

#ifdef ARM7
#define MM_A7WRAM         0x37f8000 // 32-bit bus
#define MM_A7WRAM_SZ        0x18000 // 96kb
#endif

#define MM_IO          0x4000000 // 32-bit bus

#ifdef ARM9
#define MM_PALRAM      0x5000000 // 16-bit bus
#define MM_PALRAM_SZ       0x800 // 2kb
#endif

#define MM_VRAM        0x6000000 // 16-bit bus

#ifdef ARM7
#define MM_VRAM_SZ_MAX   0x40000 // 256kb
#endif

#ifdef ARM9
#define MM_VRAM_BG_A   (MM_VRAM)
#define MM_VRAM_BG_B   (MM_VRAM+0x200000)
#define MM_VRAM_OBJ_A  (MM_VRAM+0x400000)
#define MM_VRAM_OBJ_B  (MM_VRAM+0x600000)

#define MM_VRAM_A      (MM_VRAM+0x800000)
#define MM_VRAM_A_SZ   0x20000 // 128kb

#define MM_VRAM_B      (MM_VRAM+0x820000)
#define MM_VRAM_B_SZ   0x20000 // 128kb

#define MM_VRAM_C      (MM_VRAM+0x840000)
#define MM_VRAM_C_SZ   0x20000 // 128kb

#define MM_VRAM_D      (MM_VRAM+0x860000)
#define MM_VRAM_D_SZ   0x20000 // 128kb

#define MM_VRAM_E      (MM_VRAM+0x880000)
#define MM_VRAM_E_SZ   0x10000 // 64kb

#define MM_VRAM_F      (MM_VRAM+0x890000)
#define MM_VRAM_F_SZ   0x4000 // 16kb

#define MM_VRAM_G      (MM_VRAM+0x894000)
#define MM_VRAM_G_SZ   0x4000 // 16kb

#define MM_VRAM_H      (MM_VRAM+0x898000)
#define MM_VRAM_H_SZ   0x8000 // 32kb

#define MM_VRAM_I      (MM_VRAM+0x8a0000)
#define MM_VRAM_I_SZ   0x4000 // 16kb

#define MM_OBJRAM      0x7000000 // 32-bit bus
#define MM_OBJRAM_SZ   0x800     // 2kb
#endif

#define MM_CARTROM     0x8000000 // 16-bit bus
#define MM_CARTROM_SZ  0x2000000 // 32mb

#define MM_CARTRAM     0xa000000 // 8-bit bus
#define MM_CARTRAM_SZ    0x10000 // 64kb

#define MM_MAINRAM2_TWL        0xc000000 // 16-bit bus
#define MM_MAINRAM2_TWL_SZ     0x1000000 // 16mb
#define MM_MAINRAM2_TWL_SZ_DBG 0x2000000 // 32mb

#ifdef ARM9
#define MM_BIOS        0xffff0000 // 32-bit bus
#define MM_BIOS_SZ_NTR     0x1000 // 4kb
#define MM_BIOS_SZ_TWL    0x10000 // 64kb
#endif

//! @}
