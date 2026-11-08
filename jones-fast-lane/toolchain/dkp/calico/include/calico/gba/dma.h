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

/*! @addtogroup dma
	@{
*/

/*! @name DMA registers
	@{
*/

#define REG_DMAxSAD(_x)   MK_REG(u32, IO_DMAxSAD(_x))
#define REG_DMAxDAD(_x)   MK_REG(u32, IO_DMAxDAD(_x))
#define REG_DMAxCNT(_x)   MK_REG(u32, IO_DMAxCNT(_x))
// Helper macros. REG_DMAxCNT_L doesn't make sense for NDS Arm9 as word count field is 21-bit there.
#if defined(ARM7)
#define REG_DMAxCNT_L(_x) MK_REG(u16, IO_DMAxCNT(_x)+0)
#endif
#define REG_DMAxCNT_H(_x) MK_REG(u16, IO_DMAxCNT(_x)+2)
#if defined(IO_DMAxFIL)
#define REG_DMAxFIL(_x)   MK_REG(u32, IO_DMAxFIL(_x))
#else
#define REG_DMAxFIL(_x)   __dma_fill[(_x)]
#endif

#if defined(__NDS__) && defined(ARM9)
#define DMA_WCOUNT_MASK   0x1fffffu // 21 bits
#else
#define DMA_WCOUNT_MASK   0x00ffffu // 16 bits
#endif

#define DMA_MODE_DST(_x) (((_x)&3)<<5)
#define DMA_MODE_SRC(_x) (((_x)&3)<<7)
#define DMA_MODE_REPEAT  (1<<9)
#define DMA_UNIT_16      (0<<10)
#define DMA_UNIT_32      (1<<10)
#if defined(ARM7)
#if defined(__GBA__)
#define DMA_CART_DREQ    (1<<11)
#endif
#define DMA_TIMING(_x)   (((_x)&3)<<12)
#elif defined(ARM9)
#define DMA_TIMING(_x)   (((_x)&7)<<11)
#endif
#define DMA_IRQ_ENABLE   (1<<14)
#define DMA_START        (1<<15)

//! @}

MK_EXTERN_C_START

#if !defined(IO_DMAxFIL)
//! @private
extern vu32 __dma_fill[4];
#endif

//! DMA address mode
typedef enum DmaMode {
	DmaMode_Increment  = 0, //!< Increments the address after every word/halfword
	DmaMode_Decrement  = 1, //!< Like DmaMode_Increment, but decrements instead
	DmaMode_Fixed      = 2, //!< Keeps the address fixed after every word/halfword
	DmaMode_IncrReload = 3, //!< Destination only: Like DmaMode_Increment, but reloads the original destination address after every complete transfer
} DmaMode;

//! DMA transfer timing mode
typedef enum DmaTiming {
	DmaTiming_Immediate = 0, //!< Transfer starts immediately
	DmaTiming_VBlank    = 1, //!< Transfer starts at VBlank

#if defined(__GBA__)
	DmaTiming_HBlank    = 2, //!< Transfer starts at HBlank
	DmaTiming_Special   = 3, //!< See DmaTiming_Sound and DmaTiming_DispStart

	DmaTiming_Sound     = DmaTiming_Special, //!< DMA1 & DMA2 only: Transfer is driven by sound FIFO
	DmaTiming_DispStart = DmaTiming_Special, //!< DMA3 only: Transfer is synchronized with the start of the display
#elif defined(__NDS__) && defined(ARM7)
	DmaTiming_Slot1     = 2, //!< Transfer is driven by DS gamecard
	DmaTiming_Special   = 3, //!< See DmaTiming_Mitsumi and DmaTiming_Slot2

	DmaTiming_Mitsumi   = DmaTiming_Special, //!< DMA0 & DMA2 only: Transfer is driven by Mitsumi wireless
	DmaTiming_Slot2     = DmaTiming_Special, //!< DMA1 & DMA3 only: Transfer is driven by GBA cart
#elif defined(__NDS__) && defined(ARM9)
	DmaTiming_HBlank    = 2, //!< Transfer starts at HBlank
	DmaTiming_DispStart = 3, //!< Transfer is synchronized with the start of the display
	DmaTiming_MemDisp   = 4, //!< Transfer is used to support main memory display
	DmaTiming_Slot1     = 5, //!< Transfer is driven by DS gamecard
	DmaTiming_Slot2     = 6, //!< Transfer is driven by GBA cart
	DmaTiming_3dFifo    = 7, //!< Transfer is driven by 3D geometry command FIFO
#endif

} DmaTiming;

//! Returns true if DMA channel @p id is active
MK_INLINE bool dmaIsBusy(unsigned id)
{
	return REG_DMAxCNT_H(id) & DMA_START;
}

//! Waits for DMA channel @p id to be idle, using a busy loop
MK_INLINE void dmaBusyWait(unsigned id)
{
	while (dmaIsBusy(id));
}

//! @private
MK_INLINE void _dmaSetDmaCnt(unsigned id, size_t wordCount, unsigned flags)
{
	REG_DMAxCNT(id) = ((flags << 16) & ~DMA_WCOUNT_MASK) | (wordCount & DMA_WCOUNT_MASK);
}

/*! @brief Starts a 32-bit immediate DMA transfer on channel @p id
	@param[out] dst Destination address
	@param[in] src Source address
	@param[in] size Length of the transfer in bytes
*/
MK_INLINE void dmaStartCopy32(unsigned id, void* dst, const void* src, size_t size)
{
	REG_DMAxSAD(id) = (u32)src;
	REG_DMAxDAD(id) = (u32)dst;

	size_t wordCount = size/4;
	unsigned flags =
		DMA_MODE_DST(DmaMode_Increment) |
		DMA_MODE_SRC(DmaMode_Increment) |
		DMA_UNIT_32 |
		DMA_TIMING(DmaTiming_Immediate) |
		DMA_START;

	_dmaSetDmaCnt(id, wordCount, flags);
}

/*! @brief Starts a 16-bit immediate DMA transfer on channel @p id
	@param[out] dst Destination address
	@param[in] src Source address
	@param[in] size Length of the transfer in bytes
*/
MK_INLINE void dmaStartCopy16(unsigned id, void* dst, const void* src, size_t size)
{
	REG_DMAxSAD(id) = (u32)src;
	REG_DMAxDAD(id) = (u32)dst;

	size_t wordCount = size/2;
	unsigned flags =
		DMA_MODE_DST(DmaMode_Increment) |
		DMA_MODE_SRC(DmaMode_Increment) |
		DMA_UNIT_16 |
		DMA_TIMING(DmaTiming_Immediate) |
		DMA_START;

	_dmaSetDmaCnt(id, wordCount, flags);
}

/*! @brief Starts a 32-bit immediate DMA fill on channel @p id
	@param[out] dst Destination address
	@param[in] value 32-bit fill value
	@param[in] size Length of the fill in bytes
*/
MK_INLINE void dmaStartFill32(unsigned id, void* dst, u32 value, size_t size)
{
	REG_DMAxFIL(id) = value;
	REG_DMAxSAD(id) = (u32)&REG_DMAxFIL(id);
	REG_DMAxDAD(id) = (u32)dst;

	size_t wordCount = size/4;
	unsigned flags =
		DMA_MODE_DST(DmaMode_Increment) |
		DMA_MODE_SRC(DmaMode_Fixed) |
		DMA_UNIT_32 |
		DMA_TIMING(DmaTiming_Immediate) |
		DMA_START;

	_dmaSetDmaCnt(id, wordCount, flags);
}

/*! @brief Starts a 16-bit immediate DMA fill on channel @p id
	@param[out] dst Destination address
	@param[in] value 16-bit fill value
	@param[in] size Length of the fill in bytes
*/
MK_INLINE void dmaStartFill16(unsigned id, void* dst, u16 value, size_t size)
{
	REG_DMAxFIL(id) = value;
	REG_DMAxSAD(id) = (u32)&REG_DMAxFIL(id);
	REG_DMAxDAD(id) = (u32)dst;

	size_t wordCount = size/2;
	unsigned flags =
		DMA_MODE_DST(DmaMode_Increment) |
		DMA_MODE_SRC(DmaMode_Fixed) |
		DMA_UNIT_16 |
		DMA_TIMING(DmaTiming_Immediate) |
		DMA_START;

	_dmaSetDmaCnt(id, wordCount, flags);
}

MK_EXTERN_C_END

//! @}
