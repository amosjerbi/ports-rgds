// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#include "../types.h"
#include "thread.h"
#include "mutex.h"

/*! @addtogroup sync
	@{
*/
/*! @name Condition variable
	Synchronization primitive used alongside a @ref Mutex to block one or more
	threads until a different thread both modifies a shared variable (i.e. the
	@em condition) and notifies the condition variable.
	@{
*/

MK_EXTERN_C_START

//! Condition variable object
typedef struct CondVar {
	u8 dummy; //!< @private
} CondVar;

//! Wakes up at most one thread waiting on condition variable @p cv.
void condvarSignal(CondVar* cv);

//! Wakes up all threads waiting on condition variable @p cv.
void condvarBroadcast(CondVar* cv);

//! @brief Blocks the current thread until the condition variable @p cv is awakened.
//! The Mutex @p m is atomically unlocked/locked during the wait.
void condvarWait(CondVar* cv, Mutex* m);

MK_EXTERN_C_END

//! @}

//! @}
