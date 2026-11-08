// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#if !defined(__GBA__)
#error "This header file is only for GBA"
#endif

#include "mm.h"

// Video engine
#define IO_DISPCNT  0x000
#define IO_DISPSTAT 0x004
#define IO_VCOUNT   0x006
#define IO_BG0CNT   0x008
#define IO_BG1CNT   0x00a
#define IO_BG2CNT   0x00c
#define IO_BG3CNT   0x00e
#define IO_BG0HOFS  0x010
#define IO_BG0VOFS  0x012
#define IO_BG1HOFS  0x014
#define IO_BG1VOFS  0x016
#define IO_BG2HOFS  0x018
#define IO_BG2VOFS  0x01a
#define IO_BG3HOFS  0x01c
#define IO_BG3VOFS  0x01e
#define IO_BG2PA    0x020
#define IO_BG2PB    0x022
#define IO_BG2PC    0x024
#define IO_BG2PD    0x026
#define IO_BG2X     0x028
#define IO_BG2Y     0x02c
#define IO_BG3PA    0x030
#define IO_BG3PB    0x032
#define IO_BG3PC    0x034
#define IO_BG3PD    0x036
#define IO_BG3X     0x038
#define IO_BG3Y     0x03c
#define IO_WIN0H    0x040
#define IO_WIN1H    0x042
#define IO_WIN0V    0x044
#define IO_WIN1V    0x046
#define IO_WININ    0x048
#define IO_WINOUT   0x04a
#define IO_MOSAIC   0x04c
#define IO_BLDCNT   0x050
#define IO_BLDALPHA 0x052
#define IO_BLDY     0x054

#define IO_BGxCNT(_x)  (IO_BG0CNT  + (IO_BG1CNT -IO_BG0CNT )*(_x))
#define IO_BGxHOFS(_x) (IO_BG0HOFS + (IO_BG1HOFS-IO_BG0HOFS)*(_x))
#define IO_BGxVOFS(_x) (IO_BG0VOFS + (IO_BG1VOFS-IO_BG0VOFS)*(_x))
#define IO_WINxH(_x)   (IO_WIN0H   + (IO_WIN1H  -IO_WIN0H  )*(_x))
#define IO_WINxV(_x)   (IO_WIN0V   + (IO_WIN1V  -IO_WIN0V  )*(_x))

// DMA
#define IO_DMAxSAD(_x) (0x0b0 + 0xc*(_x))
#define IO_DMAxDAD(_x) (0x0b4 + 0xc*(_x))
#define IO_DMAxCNT(_x) (0x0b8 + 0xc*(_x))

// Timer
#define IO_TMxCNT(_x) (0x100 + 0x4*(_x))

// Keypad input
#define IO_KEYINPUT 0x130
#define IO_KEYCNT   0x132

// Interrupt/system control
#define IO_IE       0x200
#define IO_IF       0x202
#define IO_WAITCNT  0x204
#define IO_IME      0x208
