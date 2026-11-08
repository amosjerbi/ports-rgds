// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#include "../types.h"
#include "thread.h"

/*! @addtogroup sync
	@{
*/
/*! @name Mutex
	The quintessential synchronization primitive, used to protect shared data
	from being simultaneously accessed by multiple threads. Calico implements
	priority inheritance for mutexes, which means that if a higher priority
	thread is waiting on a mutex held by a lower priority thread; said lower
	thread temporarily inherits the priority of the higher thread, so that the
	lower thread gets a chance to run and does not result in priority inversion.
	@{
*/

MK_EXTERN_C_START

//! @brief Mutex object
typedef struct Mutex {
	Thread* owner; //!< @private
} Mutex;

/*! @brief Recursive mutex object

	Recursive mutexes can be locked multiple times by the same thread.
	It is necessary to balance calls to @ref rmutexLock and @ref rmutexUnlock,
	or else the mutex is never released.
*/
typedef struct RMutex {
	Mutex mutex; //!< @private
	u32 counter; //!< @private
} RMutex;

//! @brief Returns true if @p m is held by the current thread
MK_INLINE bool mutexIsLockedByCurrentThread(Mutex* m)
{
	return m->owner == threadGetSelf();
}

//! @brief Attempts to lock the Mutex @p m
bool mutexTryLock(Mutex* m);

//! @brief Locks the Mutex @p m
void mutexLock(Mutex* m);

/*! @brief Unlocks the Mutex @p m
	@warning @p m **must** be held by the current thread
*/
void mutexUnlock(Mutex* m);

//! @brief Attempts to lock the RMutex @p m
MK_INLINE bool rmutexTryLock(RMutex* m)
{
	bool rc;
	if (mutexIsLockedByCurrentThread(&m->mutex)) {
		rc = true;
		m->counter ++;
	} else {
		rc = mutexTryLock(&m->mutex);
		if (rc) {
			m->counter = 1;
		}
	}
	return rc;
}

//! @brief Locks the RMutex @p m
MK_INLINE void rmutexLock(RMutex* m)
{
	if (mutexIsLockedByCurrentThread(&m->mutex)) {
		m->counter ++;
	} else {
		mutexLock(&m->mutex);
		m->counter = 1;
	}
}

//! @brief Unlocks the RMutex @p m
MK_INLINE void rmutexUnlock(RMutex* m)
{
	if (!--m->counter) {
		mutexUnlock(&m->mutex);
	}
}

MK_EXTERN_C_END

//! @}

//! @}
