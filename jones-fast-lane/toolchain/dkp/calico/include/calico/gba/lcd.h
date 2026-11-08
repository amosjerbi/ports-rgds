// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#include "../types.h"

#if defined(__GBA__)
#include "io.h"
#elif defined(__NDS__)
#include "../nds/io.h"
#else
#error "This header file is only for GBA and NDS"
#endif

/*! @addtogroup lcd
	@{
*/

#if defined(__GBA__)
#define LCD_WIDTH  240 //!< Width of the LCD in pixels
#define LCD_HEIGHT 160 //!< Height of the LCD in pixels
#elif defined(__NDS__)
#define LCD_WIDTH  256 //!< Width of the LCD in pixels
#define LCD_HEIGHT 192 //!< Height of the LCD in pixels
#endif

/*! @name LCD memory mapped I/O
	@{
*/

#define REG_DISPSTAT MK_REG(u16, IO_DISPSTAT)
#define REG_VCOUNT   MK_REG(u16, IO_VCOUNT)

#define DISPSTAT_IF_VBLANK (1U<<0)
#define DISPSTAT_IF_HBLANK (1U<<1)
#define DISPSTAT_IF_VCOUNT (1U<<2)
#define DISPSTAT_IE_VBLANK (1U<<3)
#define DISPSTAT_IE_HBLANK (1U<<4)
#define DISPSTAT_IE_VCOUNT (1U<<5)
#define DISPSTAT_IE_ALL    (7U<<3)
#if defined(__GBA__)
#define DISPSTAT_LYC_MASK  (0xff<<8)
#define DISPSTAT_LYC(_x)   (((_x)&0xff)<<8)
#elif defined(__NDS__)
#define DISPSTAT_LCD_READY_TWL (1U<<6)
#define DISPSTAT_LYC_MASK  (0x1ff<<7)
#define DISPSTAT_LYC(_x)   _lcdCalcLyc(_x)

//! @}

//! @private
MK_CONSTEXPR unsigned _lcdCalcLyc(unsigned lyc)
{
	return ((lyc&0xff) << 8) | (((lyc>>8)&1) << 7);
}
#endif

MK_EXTERN_C_START

/*! @brief Enables or disables LCD interrupts
	@param[in] mask Bitmask of LCD interrupts to modify
	@param[in] value New value for LCD interrupt bits modified by @p mask
	@note The bits that can be configured are: `DISPSTAT_IE_VBLANK`, `DISPSTAT_IE_HBLANK`, `DISPSTAT_IE_VCOUNT`
*/
MK_INLINE void lcdSetIrqMask(unsigned mask, unsigned value)
{
	mask &= DISPSTAT_IE_ALL;
	REG_DISPSTAT = (REG_DISPSTAT &~ mask) | (value & mask);
}

//! Enables or disables the VBlank interrupt
MK_INLINE void lcdSetVBlankIrq(bool enable)
{
	lcdSetIrqMask(DISPSTAT_IE_VBLANK, enable ? DISPSTAT_IE_VBLANK : 0);
}

//! Enables or disables the HBlank interrupt
MK_INLINE void lcdSetHBlankIrq(bool enable)
{
	lcdSetIrqMask(DISPSTAT_IE_HBLANK, enable ? DISPSTAT_IE_HBLANK : 0);
}

/*! @brief Configures the VCount interrupt
	@param[in] enable true to enable, false to disable
	@param[in] lyc Scanline which will trigger the VCount interrupt
*/
MK_INLINE void lcdSetVCountCompare(bool enable, unsigned lyc)
{
	unsigned reg = REG_DISPSTAT &~ (DISPSTAT_IE_VCOUNT|DISPSTAT_LYC_MASK);
	if (enable) {
		reg |= DISPSTAT_IE_VCOUNT | DISPSTAT_LYC(lyc);
	}
	REG_DISPSTAT = reg;
}

//! Returns true if the LCD is in the VBlank period
MK_INLINE bool lcdInVBlank(void)
{
	return REG_DISPSTAT & DISPSTAT_IF_VBLANK;
}

//! Returns true if the LCD is in the HBlank period
MK_INLINE bool lcdInHBlank(void)
{
	return REG_DISPSTAT & DISPSTAT_IF_HBLANK;
}

//! Returns true if the current LCD scanline matches the one configured by @ref lcdSetVCountCompare
MK_INLINE bool lcdInVCountMatch(void)
{
	return REG_DISPSTAT & DISPSTAT_IF_VCOUNT;
}

//! Returns the current LCD scanline
MK_INLINE unsigned lcdGetVCount(void)
{
	return REG_VCOUNT;
}

MK_EXTERN_C_END

//! @}
