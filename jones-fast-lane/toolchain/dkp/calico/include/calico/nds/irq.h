// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#if !defined(__NDS__)
#error "This header file is only for NDS"
#endif

#include "../types.h"
#include "../arm/common.h"
#include "io.h"

/*! @addtogroup irq
	@{
*/

/*! @name Interrupt controller memory mapped I/O
	@{
*/

#define REG_IME MK_REG(u32, IO_IME)
#define REG_IE  MK_REG(u32, IO_IE)
#define REG_IF  MK_REG(u32, IO_IF)
#ifdef ARM7
#define REG_IE2 MK_REG(u32, IO_IE2)
#define REG_IF2 MK_REG(u32, IO_IF2)
#endif

//! @}

/*! @name Interrupt bits
	@{
*/

#define IRQ_VBLANK       (1U << 0)  //!< @ref lcd vertical blank interrupt
#define IRQ_HBLANK       (1U << 1)  //!< @ref lcd horizontal blank interrupt
#define IRQ_VCOUNT       (1U << 2)  //!< @ref lcd VCount match interrupt
#define IRQ_TIMER0       (1U << 3)  //!< @ref timer channel 0 interrupt
#define IRQ_TIMER1       (1U << 4)  //!< @ref timer channel 1 interrupt
#define IRQ_TIMER2       (1U << 5)  //!< @ref timer channel 2 interrupt
#define IRQ_TIMER3       (1U << 6)  //!< @ref timer channel 3 interrupt
#define IRQ_DMA0         (1U << 8)  //!< @ref dma channel 0 interrupt
#define IRQ_DMA1         (1U << 9)  //!< @ref dma channel 1 interrupt
#define IRQ_DMA2         (1U << 10) //!< @ref dma channel 2 interrupt
#define IRQ_DMA3         (1U << 11) //!< @ref dma channel 3 interrupt
#define IRQ_KEYPAD       (1U << 12) //!< @ref keypad interrupt
#define IRQ_SLOT2        (1U << 13) //!< @ref gbacart interrupt
#define IRQ_SLOT1_DET    (1U << 14) //!< @ref ntrcard detect interrupt (DSi-only)
#define IRQ_PXI_SYNC     (1U << 16) //!< @ref pxi synchronization (ping) interrupt
#define IRQ_PXI_SEND     (1U << 17) //!< @ref pxi send interrupt
#define IRQ_PXI_RECV     (1U << 18) //!< @ref pxi receive interrupt
#define IRQ_SLOT1_TX     (1U << 19) //!< @ref ntrcard transfer interrupt
#define IRQ_SLOT1_IREQ   (1U << 20) //!< @ref ntrcard device interrupt
#define IRQ_NDMA0        (1U << 28) //!< @ref ndma channel 0 interrupt
#define IRQ_NDMA1        (1U << 29) //!< @ref ndma channel 1 interrupt
#define IRQ_NDMA2        (1U << 30) //!< @ref ndma channel 2 interrupt
#define IRQ_NDMA3        (1U << 31) //!< @ref ndma channel 3 interrupt

#define IRQ_TIMER(_x)    (1U << ( 3+(_x))) //!< @ref timer channel @p _x interrupt
#define IRQ_DMA(_x)      (1U << ( 8+(_x))) //!< @ref dma channel @p _x interrupt
#define IRQ_NDMA(_x)     (1U << (28+(_x))) //!< @ref ndma channel @p _x interrupt

#ifdef ARM7
#define MK_IRQ_NUM_HANDLERS 64

#define IRQ_RTC          (1U << 7)
#define IRQ_HINGE        (1U << 22)
#define IRQ_SPI          (1U << 23)
#define IRQ_WIFI         (1U << 24)

#define IRQ2_HEADPHONE   (1U << 5)
#define IRQ2_MCU         (1U << 6)
#define IRQ2_TMIO0       (1U << 8)
#define IRQ2_TMIO0_SDIO  (1U << 9)
#define IRQ2_TMIO1       (1U << 10)
#define IRQ2_TMIO1_SDIO  (1U << 11)
#define IRQ2_AES         (1U << 12)
#define IRQ2_I2C         (1U << 13)
#define IRQ2_MICEX       (1U << 14)
#endif

#ifdef ARM9
#define MK_IRQ_NUM_HANDLERS 32

#define IRQ_3DFIFO       (1U << 21) //!< 3D Geometry Engine FIFO interrupt (ARM9-only)
#define IRQ_DSP          (1U << 24) //!< DSP interrupt (ARM9, DSi-only)
#define IRQ_CAM          (1U << 25) //!< Camera interrupt (ARM9, DSi-only)
#endif

//! @}

//! Saved state of `REG_IME`
typedef unsigned IrqState;

//! Interrupt mask datatype
typedef u32 IrqMask;

/*! @brief Temporarily disables interrupts on the interrupt controller (using `REG_IME`).
	@return Previous @ref IrqState, to be later passed to @ref irqUnlock
	@note @ref irqLock and @ref irqUnlock are best suited for simple atomic operations
	that involve no external function calls, especially in THUMB-mode code.
	@warning `REG_IME` is **not** part of the thread context, and is therefore not saved/restored
	during context switch. The threading subsystem **will malfunction** if any context switches
	occur while it is disabled. Consider using @ref armIrqLockByPsr and @ref armIrqUnlockByPsr
	instead if you intend to implement synchronization code, or call external functions.
*/
MK_INLINE IrqState irqLock(void)
{
	armCompilerBarrier();
	IrqState saved = REG_IME;
	REG_IME = 0;
	armCompilerBarrier();
	return saved;
}

//! @brief Restores the previous interrupt @p state locked by @ref irqLock
MK_INLINE void irqUnlock(IrqState state)
{
	armCompilerBarrier();
	REG_IME = state;
	armCompilerBarrier();
}

//! @}
