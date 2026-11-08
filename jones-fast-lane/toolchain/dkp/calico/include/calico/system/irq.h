// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#include "../types.h"

#if defined(__GBA__)
#include "../gba/irq.h"
#elif defined(__NDS__)
#include "../nds/irq.h"
#else
#error "Unsupported platform."
#endif

/*! @addtogroup irq
	@{
*/

MK_EXTERN_C_START

/*! @brief Interrupt service routine (ISR) function pointer datatype
	@note ISRs execute in a special mode of the ARM CPU called <em>interrupt mode</em>,
	or IRQ mode for short. This mode has its own dedicated stack independent of any
	currently running thread. Interrupts are disabled while executing ISRs, meaning
	they are unable to nest. Please exercise caution when writing an interrupt handler.
	Do **not** call threading functions, C standard library functions, or access thread
	local variables from within ISR mode. As an exception, it is possible to call
	threadUnblock\* functions or @ref mailboxTrySend in order to wake up threads in
	response to hardware events. These threads should be the ones in charge of performing
	any heavy processing, instead of doing it from within the ISR.
*/
typedef void (*IrqHandler)(void);

//! @private
extern volatile IrqMask __irq_flags;

/*! @brief Assigns an interrupt service routine (ISR) to one or more interrupts
	@param[in] mask Bitmask of interrupts to which assign the ISR
	@param[in] handler Pointer to ISR (see @ref IrqHandler)
	@note There can only be one ISR assigned to a given interrupt at a time.
	Assigning a different routine to the same interrupt will replace the previous one.
*/
void irqSet(IrqMask mask, IrqHandler handler);

//! Removes all ISRs registered for all interrupts in the given @p mask
MK_INLINE void irqClear(IrqMask mask)
{
	irqSet(mask, NULL);
}

//! Enables all interrupts selected by the given @p mask
MK_INLINE void irqEnable(IrqMask mask)
{
	IrqState st = irqLock();
	REG_IE |= mask;
	irqUnlock(st);
}

//! Disables all interrupts selected by the given @p mask
MK_INLINE void irqDisable(IrqMask mask)
{
	IrqState st = irqLock();
	REG_IE &= ~mask;
	irqUnlock(st);
}

#if MK_IRQ_NUM_HANDLERS > 32

//! @private
extern volatile IrqMask __irq_flags2;

//! Like @ref irqSet, but for the extended DSi ARM7 interrupt controller
void irqSet2(IrqMask mask, IrqHandler handler);

//! Like @ref irqClear, but for the extended DSi ARM7 interrupt controller
MK_INLINE void irqClear2(IrqMask mask)
{
	irqSet2(mask, NULL);
}

//! Like @ref irqEnable, but for the extended DSi ARM7 interrupt controller
MK_INLINE void irqEnable2(IrqMask mask)
{
	IrqState st = irqLock();
	REG_IE2 |= mask;
	irqUnlock(st);
}

//! Like @ref irqDisable, but for the extended DSi ARM7 interrupt controller
MK_INLINE void irqDisable2(IrqMask mask)
{
	IrqState st = irqLock();
	REG_IE2 &= ~mask;
	irqUnlock(st);
}

#endif

MK_EXTERN_C_END

//! @}
