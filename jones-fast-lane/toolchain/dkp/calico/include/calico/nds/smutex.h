// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#include "../types.h"

/*! @addtogroup sync
	@{
*/
/*! @name System-wide mutex (or "Super-mutex")
	Provides a mutex that can be used to arbitrate accesses to resources between
	the ARM9 and the ARM7. Unlike @ref Mutex, this primitive does not implement
	priority inheritance.
	@{
*/

#if defined(ARM9)
#define SMUTEX_MY_CPU_ID 9
#elif defined(ARM7)
#define SMUTEX_MY_CPU_ID 7
#else
#error "ARM9 or ARM7 must be defined"
#endif

MK_EXTERN_C_START

//! System-wide mutex object
typedef struct SMutex {
	u32 spinner;          //!< @private
	u32 thread_ptr : 28;  //!< @private
	u32 cpu_id : 4;       //!< @private
} SMutex;

//! @brief Locks the SMutex @p m
MK_EXTERN32 void smutexLock(SMutex* m);

//! @brief Tries to lock the SMutex @p m without blocking, returning true on success
MK_EXTERN32 bool smutexTryLock(SMutex* m);

//! @brief Unlocks the SMutex @p m
MK_EXTERN32 void smutexUnlock(SMutex* m);

MK_EXTERN_C_END

//! @}

//! @}
