// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#include "../types.h"
#include "sysclock.h"

/*! @addtogroup tick
	@{
*/

//! Frequency in Hz of the system timer used for counting ticks
#define TICK_FREQ (SYSTEM_CLOCK/64)

MK_EXTERN_C_START

// Forward declaration
typedef struct TickTask TickTask;

/*! @brief Tick task callback function
	@param[in] t Pointer to @ref TickTask object that triggered the event
	@note The callback runs in IRQ mode - exercise caution!
	See @ref IrqHandler for more details on how to write IRQ mode handlers.
*/
typedef void (* TickTaskFn)(TickTask* t);

//! Tick task object, representing a scheduled timed event
struct TickTask {
	TickTask* next; //!< @private
	u32 target;     //!< @private
	u32 period;     //!< @private
	TickTaskFn fn;  //!< @private
};

//! Converts microseconds (@p us) to system ticks
MK_CONSTEXPR u32 ticksFromUsec(u32 us)
{
	return (us * (u64)TICK_FREQ) / 1000000;
}

//! Converts frequency in @p hz to a period measured in system ticks
MK_CONSTEXPR u32 ticksFromHz(u32 hz)
{
	return (TICK_FREQ + hz/2) / hz;
}

//! @private
void tickInit(void);

//! Returns the current value of the system tick counter
u64 tickGetCount(void);

/*! @brief Configures and starts a tick task @p t
	@param[in] fn Event callback to invoke when the tick task needs to run.
	@param[in] delay_ticks Time to wait (in system ticks) for the first invocation of the task.
	@param[in] period_ticks For periodic tasks, specifies the interval (in system ticks) between invocations. Pass 0 for one-shot tasks.
	@note Use @ref ticksFromUsec and @ref ticksFromHz to convert from microseconds/Hz into system ticks
*/
void tickTaskStart(TickTask* t, TickTaskFn fn, u32 delay_ticks, u32 period_ticks);

//! Stops the tick task @p t
void tickTaskStop(TickTask* t);

MK_EXTERN_C_END

//! @}
