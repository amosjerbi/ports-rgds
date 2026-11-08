// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#if __ARM_ARCH < 5
#error "This header is for ARMv5+ only"
#endif

#include "../types.h"
#include "cp15.h"

/*! @addtogroup cache
	@{

	@note Cache operations by address range are **always** performed on cache line boundaries.
	This means that the start of the given range is rounded down (`addr &~ 0x1f`), while
	the end of the range is rounded up (`(addr + size + 0x1f) &~ 0x1f`).
*/

MK_EXTERN_C_START

/*! @brief Drains the CPU's write buffer, making changes observable to other devices (e.g. ARM7 or DMA).
	@note It is usually not necessary to use this barrier because the data cache management functions
	(such as @ref armDCacheFlush) already include it.
	In addition, uncacheable MPU regions do not usually have the write buffer enabled anyway.
	@note In later versions of the ARM architecture (v6+), this is known as a Data Synchronization Barrier (DSB),
	and it is also in charge of preventing memory access reordering.
*/
void armDrainWriteBuffer(void);

//! @brief Flushes (cleans and invalidates) the entire data cache
void armDCacheFlushAll(void);

/*! @brief Flushes (cleans and invalidates) the data cache lines pertaining to the specified address range
	@param addr Start address (any pointer type)
	@param size Size of the address range
	@note Use this function when sharing a main RAM memory buffer with the ARM7, or with DMA/devices.
	@note Consider using @ref armDCacheFlushAll when `size` is large.
	As an example, the 3DS ARM9 kernel uses 16 KiB as the threshold.
*/
void armDCacheFlush(const volatile void* addr, size_t size);

/*! @brief Invalidates the data cache lines pertaining to the specified address range
	@param addr Start address (any pointer type)
	@param size Size of the address range
	@warning Data cache invalidation is rarely useful, and **will** lead to data corruption when used incorrectly.
	Prefer using @ref armDCacheFlush, unless the address range is guaranteed to be aligned to cache line boundaries
	and it is explicitly permitted to discard unflushed writes within the range.
*/
void armDCacheInvalidate(const volatile void* addr, size_t size);

//! @brief Invalidates the entire instruction cache
void armICacheInvalidateAll(void);

/*! @brief Invalidates the instruction cache lines pertaining to the specified address range
	@param addr Start address (any pointer type)
	@param size Size of the address range
	@note Use this function when dynamically loading/generating code (do not forget to use @ref armDCacheFlush too).
	@note Consider using @ref armICacheInvalidateAll unless `size` is known to be small.
*/
void armICacheInvalidate(const volatile void* addr, size_t size);

MK_EXTERN_C_END

//! @}
