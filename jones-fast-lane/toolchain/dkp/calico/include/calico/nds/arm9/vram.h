// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#if !defined(__NDS__) || !defined(ARM9)
#error "This header file is only for NDS ARM9"
#endif

#include "../../types.h"
#include "../io.h"

/*! @addtogroup hw
	@{
*/

/*! @name VRAM configuration registers
	@{
*/

#define REG_VRAMCNT_A MK_REG(u8, IO_VRAMCNT_A)
#define REG_VRAMCNT_B MK_REG(u8, IO_VRAMCNT_B)
#define REG_VRAMCNT_C MK_REG(u8, IO_VRAMCNT_C)
#define REG_VRAMCNT_D MK_REG(u8, IO_VRAMCNT_D)
#define REG_VRAMCNT_E MK_REG(u8, IO_VRAMCNT_E)
#define REG_VRAMCNT_F MK_REG(u8, IO_VRAMCNT_F)
#define REG_VRAMCNT_G MK_REG(u8, IO_VRAMCNT_G)
#define REG_VRAMCNT_H MK_REG(u8, IO_VRAMCNT_H)
#define REG_VRAMCNT_I MK_REG(u8, IO_VRAMCNT_I)

#define REG_VRAMCNT_ABCD MK_REG(u32, IO_VRAMCNT_A)
#define REG_VRAMCNT_AB   MK_REG(u16, IO_VRAMCNT_A)
#define REG_VRAMCNT_CD   MK_REG(u16, IO_VRAMCNT_C)
#define REG_VRAMCNT_EF   MK_REG(u16, IO_VRAMCNT_E)
#define REG_VRAMCNT_HI   MK_REG(u16, IO_VRAMCNT_H)

#define VRAM_ENABLE     (1U<<7)
#define VRAM_MST(_x)    ((_x)&7)
#define VRAM_OFFSET(_x) (((_x)&3)<<3)
#define VRAM_CONFIG(_mst, _off) (VRAM_MST(_mst) | VRAM_OFFSET(_off) | VRAM_ENABLE)

//! @}

//! @}
