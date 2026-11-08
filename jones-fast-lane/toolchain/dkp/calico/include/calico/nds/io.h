// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#if !defined(__NDS__)
#error "This header file is only for NDS"
#endif

#include "mm.h"

/*! @addtogroup io
	@{
*/

// Basic video
#define IO_DISPSTAT 0x004
#define IO_VCOUNT   0x006

// Video engines
#ifdef ARM9
#define IO_GFX_A        0x0000
#define IO_GFX_B        0x1000

// Engine A/B
#define IO_DISPCNT       0x000
#define IO_BGxCNT(_x)   (0x008 + 0x2*(_x))
#define IO_BGxHOFS(_x)  (0x010 + 0x4*(_x))
#define IO_BGxVOFS(_x)  (0x012 + 0x4*(_x))
#define IO_BG2xPA(_x)   (0x020 + 0x10*(_x))
#define IO_BG2xPB(_x)   (0x022 + 0x10*(_x))
#define IO_BG2xPC(_x)   (0x024 + 0x10*(_x))
#define IO_BG2xPD(_x)   (0x026 + 0x10*(_x))
#define IO_BG2xX(_x)    (0x028 + 0x10*(_x))
#define IO_BG2xY(_x)    (0x02c + 0x10*(_x))
#define IO_WINxH(_x)    (0x040 + 0x2*(_x))
#define IO_WINxV(_x)    (0x044 + 0x2*(_x))
#define IO_WININ         0x048
#define IO_WINOUT        0x04a
#define IO_MOSAIC        0x04c
#define IO_BLDCNT        0x050
#define IO_BLDALPHA      0x052
#define IO_BLDY          0x054
#define IO_MASTER_BRIGHT 0x06c

// Engine A only
#define IO_DISP3DCNT      0x060
#define IO_DISPCAPCNT     0x064
#define IO_DISP_MMEM_FIFO 0x068
#define IO_TVOUTCNT       0x070 // Actually usable on DS Lite with a special hardmod!
#endif

// DMA
#define IO_DMAxSAD(_x) (0x0b0 + 0xc*(_x))
#define IO_DMAxDAD(_x) (0x0b4 + 0xc*(_x))
#define IO_DMAxCNT(_x) (0x0b8 + 0xc*(_x))
#ifdef ARM9
#define IO_DMAxFIL(_x) (0x0e0 + 0x4*(_x))
#endif

// Timer
#define IO_TMxCNT(_x) (0x100 + 0x4*(_x))

// General Purpose IO (GPIO), including keypad input
#ifdef ARM7
// XX: theoretical/vestigial SIO registers at 0x120..0x12f
#endif
#define IO_KEYINPUT 0x130
#define IO_KEYCNT   0x132
#ifdef ARM7
#define IO_RCNT     0x134 // Inherited from GBA (SC/SD/SI/SO lines). On DS only meaningful in GPIO mode (no SIO, no JOY). RTC IRQ is connected to SI.
#define IO_RCNT_EXT 0x136 // 8 extra GPIO lines (input-only), added to above. X/Y buttons are connected here.
#define IO_RCNT_RTC 0x138 // 8 more GPIO lines (input/output, bottom 3 of which are used to bitbang the RTC IC).
// XX: theoretical/vestigial JOY registers at 0x140..0x15f (?)
#endif

// PXI
#define IO_PXI_SYNC 0x180
#define IO_PXI_CNT  0x184
#define IO_PXI_SEND 0x188
#define IO_PXI_RECV 0x100000 // LOL NINTENDO WHY ISN'T THIS 18C

// DS Gamecard
#define IO_NTRCARD_CNT      0x1a0
#define IO_NTRCARD_SPIDATA  0x1a2
#define IO_NTRCARD_ROMCNT   0x1a4
#define IO_NTRCARD_ROMCMD   0x1a8
#define IO_NTRCARD_SEED0_LO 0x1b0
#define IO_NTRCARD_SEED1_LO 0x1b4
#define IO_NTRCARD_SEED0_HI 0x1b8
#define IO_NTRCARD_SEED1_HI 0x1ba
#define IO_NTRCARD_FIFO     0x100010 // LOL NINTENDO THIS COULD'VE BEEN 1BC

// SPI
#ifdef ARM7
#define IO_SPICNT   0x1c0
#define IO_SPIDATA  0x1c2
#endif

// Interrupt/system control
#define IO_EXMEMCNT 0x204
#ifdef ARM7
#define IO_EXMEMCNT2 0x206
#endif
#define IO_IME      0x208
#define IO_IE       0x210
#define IO_IF       0x214
#ifdef ARM7
// DSi extended interrupts
#define IO_IE2      0x218
#define IO_IF2      0x21C
#endif

// Memory control
#ifdef ARM9
#define IO_VRAMCNT_A 0x240
#define IO_VRAMCNT_B 0x241
#define IO_VRAMCNT_C 0x242
#define IO_VRAMCNT_D 0x243
#define IO_VRAMCNT_E 0x244
#define IO_VRAMCNT_F 0x245
#define IO_VRAMCNT_G 0x246
#define IO_WRAMCNT   0x247
#define IO_VRAMCNT_H 0x248
#define IO_VRAMCNT_I 0x249
#endif
#ifdef ARM7
#define IO_VRAMSTAT  0x240
#define IO_WRAMSTAT  0x241
#endif

// Coprocessor
#ifdef ARM9
#define IO_DIVCNT    0x280
#define IO_DIV_NUMER 0x290
#define IO_DIV_DENOM 0x298
#define IO_DIV_QUOT  0x2a0
#define IO_DIV_REM   0x2a8

#define IO_SQRTCNT   0x2b0
#define IO_SQRT_OUT  0x2b4
#define IO_SQRT_IN   0x2b8
#endif

// Power management
#ifdef ARM7
#define IO_POSTFLG7  0x300
#define IO_HALTCNT   0x301
#define IO_POWCNT7   0x304
#define IO_BIOSPROT  0x308
#endif
#ifdef ARM9
#define IO_POSTFLG9  0x300
#define IO_POWCNT9   0x304
#endif

// 3D Engine (ARM9 0x320-0x6a3, todo)

// Sound
#ifdef ARM7
#define IO_SOUNDxCNT(_x)  (0x400 + 0x10*(_x))
#define IO_SOUNDxSAD(_x)  (0x404 + 0x10*(_x))
#define IO_SOUNDxTMR(_x)  (0x408 + 0x10*(_x))
#define IO_SOUNDxPNT(_x)  (0x40a + 0x10*(_x))
#define IO_SOUNDxLEN(_x)  (0x40c + 0x10*(_x))
#define IO_SOUNDCNT        0x500
#define IO_SOUNDBIAS       0x504
#define IO_SNDCAPxCNT(_x) (0x508 + (_x))
#define IO_SNDCAPxDAD(_x) (0x510 + 0x8*(_x))
#define IO_SNDCAPxLEN(_x) (0x514 + 0x8*(_x))
#endif

// Mitsumi Wireless (essentially functions like a GBA slot)
#ifdef ARM7
#define IO_MITSUMI_WS0 0x800000
#define IO_MITSUMI_WS1 0x808000
#endif

// -------- DSi exclusive registers below --------

// Vestigial second gamecard slot (DSi prototype)
// Offset can be added to all IO_CARD* regs, even the weirdo FIFO reg with a weird address
#define IO_CARD2 0x2000

// System/SoC configuration
#define IO_SCFG_ROM   0x4000
#define IO_SCFG_CLK   0x4004
#ifdef ARM9
#define IO_SCFG_RST   0x4006
#endif
#ifdef ARM7
#define IO_SCFG_JTAG  0x4006
#endif
#define IO_SCFG_EXT   0x4008
#define IO_SCFG_MC    0x4010
#ifdef ARM7
#define IO_SCFG_MCINS 0x4012
#define IO_SCFG_MCREM 0x4014
#define IO_SCFG_WL    0x4020
#define IO_SCFG_OP    0x4024
#endif

// New WRAM configuration
#define IO_MBK_SLOT_Ax(_x) (0x4040 + (_x)) // aka MBK1
#define IO_MBK_SLOT_Bx(_x) (0x4044 + (_x)) // aka MBK2/3
#define IO_MBK_SLOT_Cx(_x) (0x404c + (_x)) // aka MBK4/5
#define IO_MBK_MAP_A        0x4054         // aka MBK6
#define IO_MBK_MAP_B        0x4058         // aka MBK7
#define IO_MBK_MAP_C        0x405c         // aka MBK8
#define IO_MBK_SLOTWRPROT   0x4060         // aka MBK9

// New DMA
#define IO_NDMAGCNT        0x4100
#define IO_NDMAxSAD(_x)   (0x4104 + 0x1c*(_x))
#define IO_NDMAxDAD(_x)   (0x4108 + 0x1c*(_x))
#define IO_NDMAxTCNT(_x)  (0x410c + 0x1c*(_x))
#define IO_NDMAxWCNT(_x)  (0x4110 + 0x1c*(_x))
#define IO_NDMAxBCNT(_x)  (0x4114 + 0x1c*(_x))
#define IO_NDMAxFDATA(_x) (0x4118 + 0x1c*(_x))
#define IO_NDMAxCNT(_x)   (0x411c + 0x1c*(_x))

// ARM9-exclusive peripherals
#ifdef ARM9
// Camera (TODO, 0x4200)

// DSP (TODO, 0x4300)
#endif

// ARM7-exclusive peripherals
#ifdef ARM7

// Hardware AES (little-endian)
#define IO_AES_CNT    0x4400
#define IO_AES_LEN    0x4404
#define IO_AES_WRFIFO 0x4408
#define IO_AES_RDFIFO 0x440c
#define IO_AES_IV     0x4420
#define IO_AES_MAC    0x4430
#define IO_AES_SLOTxKEY(_x) (0x4440 + 0x30*(_x))
#define IO_AES_SLOTxX(_x)   (0x4450 + 0x30*(_x))
#define IO_AES_SLOTxY(_x)   (0x4460 + 0x30*(_x))

// I2C
#define IO_I2C_DATA 0x4500
#define IO_I2C_CNT  0x4501

// Extended microphone
#define IO_MICEX_CNT  0x4600
#define IO_MICEX_DATA 0x4604

// Extended sound
#define IO_SNDEXCNT   0x4700

// Toshiba Mobile IO controllers (one for SD/eMMC, another for Atheros Wireless SDIO)
#define IO_TMIO0_BASE 0x4800
#define IO_TMIO0_FIFO 0x490c
#define IO_TMIO1_BASE 0x4a00
#define IO_TMIO1_FIFO 0x4b0c

// Extended GPIO - A bit of explanation:
// - Bits 0..2 are GPIO18[0..2] (all lines seem fully unused?)
// - Bits 4..7 are GPIO33[0..3], out of which:
//   * GPIO33[0] seems unused
//   * GPIO33[1] is connected to headphone connection state
//   * GPIO33[2] is connected to the MCU's interrupt output
//   * GPIO33[3] is connected to sound enable output
// These same bits correspond to the first 8 extended interrupts.
#define IO_GPIO_CNT 0x4c00 // lsb: data, msb: direction
#define IO_GPIO_IRQ 0x4c02 // lsb: edge, msb: enable
#define IO_GPIO_WL  0x4c04 // lsb: /reset, msb: atheros/mitsumi switch

// SoC one-time programmable memory
#define IO_OTP_CID   0x4d00
#define IO_OTP_ERROR 0x4d08

#endif

//! @}
