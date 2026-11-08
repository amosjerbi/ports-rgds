// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#include "../types.h"
#include "psr.h"
#if __ARM_ARCH >= 5
#include "cp15.h"
#endif

/*! @addtogroup arm
	@{
*/

MK_EXTERN_C_START

//! Arm CPU register context (GPRs and SPRs)
typedef struct ArmContext {
	u32 r[16];      //!< General Purpose Registers (R0-R15)
	u32 psr;        //!< Program Status Register (CPSR/SPSR)
	u32 sp_svc;     //!< SVC-mode banked SP Register (used by BIOS)
} ArmContext;

//! Saved state of the CPSR IRQ/FIQ mask bits @see ARM_PSR_I, ARM_PSR_F
typedef unsigned ArmIrqState;

/*! @brief Prevents the compiler from reordering memory accesses around the call to this function
	@note This barrier is intended to be used when the compiler has no way to infer that the code
	may be interrupted by other events and thus cause memory locations to be updated, or when other
	processors/devices are intended to observe said updates. Compiler barriers also implicitly occur
	when calling an external (non-inline) function.
*/
MK_INLINE void armCompilerBarrier(void)
{
	__asm__ __volatile__ ("" ::: "memory");
}

/*! @brief Performs a soft breakpoint (does nothing on real hardware).
	@note This inserts a special no-op instruction recognized by some emulators such as no$gba,
	intended to aid debugging.
	It has no effect on real hardware, hence the name - not to be confused with software
	breakpoint instructions (bkpt), which do trigger a Prefetch Abort exception.
*/
MK_INLINE void armSoftBreakpoint(void)
{
	__asm__ __volatile__ ("mov r11, r11");
}

#if !__thumb__

//! @brief Retrieves the value of the Current Program Status Register
MK_EXTINLINE u32 armGetCpsr(void)
{
	u32 reg;
	__asm__ __volatile__ ("mrs %0, cpsr" : "=r" (reg));
	return reg;
}

//! @brief Sets the control bits (execution mode, IRQ/FIQ mask) of the Current Program Status Register
MK_EXTINLINE void armSetCpsrC(u32 value)
{
	__asm__ __volatile__ ("msr cpsr_c, %0" :: "r" (value) : "memory");
}

//! @brief Retrieves the value of the Saved Program Status Register
MK_EXTINLINE u32 armGetSpsr(void)
{
	u32 reg;
	__asm__ __volatile__ ("mrs %0, spsr" : "=r" (reg));
	return reg;
}

//! @brief Sets the value of the Saved Program Status Register
MK_EXTINLINE void armSetSpsr(u32 value)
{
	__asm__ __volatile__ ("msr spsr, %0" :: "r" (value));
}

/*! @brief Atomically swaps a 32-bit @p value with that at a memory location @p addr
	@warning This is not a compare-and-swap primitive - it *always* swaps.
	As such, it cannot be used to implement arbitrary atomic operations.
	@note For inter-processor synchronization consider using @ref SMutex.
	@returns The previous value of `*addr`
*/
MK_EXTINLINE u32 armSwapWord(u32 value, vu32* addr)
{
	u32 ret;
	__asm__ __volatile__ ("swp %[Rd], %[Rm], [%[Rn]]" : [Rd]"=r"(ret) : [Rm]"[Rd]"(value), [Rn]"r"(addr) : "memory");
	return ret;
}

/*! @brief Atomically swaps an 8-bit @p value with that at a memory location @p addr
	@warning This is not a compare-and-swap primitive - it *always* swaps.
	As such, it cannot be used to implement arbitrary atomic operations.
	@returns The previous value of `*addr`
*/
MK_EXTINLINE u8 armSwapByte(u8 value, vu8* addr)
{
	u8 ret;
	__asm__ __volatile__ ("swpb %[Rd], %[Rm], [%[Rn]]" : [Rd]"=r"(ret) : [Rm]"[Rd]"(value), [Rn]"r"(addr) : "memory");
	return ret;
}

#if __ARM_ARCH >= 5

/*! @brief Waits (in low-power mode) for an interrupt to be asserted on the CPU
	@note The interrupt masking bits (CPSR I/F) are ignored, however the interrupt vector
	is only executed when I/F are unmasked. If the interrupt controller is configured to
	disable all interrupts (IME=0 or IE=0), the CPU effectively stops forever
	and cannot be recovered without a full system reset.
	@warning Using this function prevents lower priority threads from executing because
	the caller thread never actually yields control of the CPU. Prefer using @ref threadIrqWait
	or @ref threadYield instead.
*/
MK_EXTINLINE void armWaitForIrq(void)
{
	__asm__ __volatile__ ("mcr p15, 0, r0, c7, c0, 4" ::: "memory");
}

//! @brief Retrieves the value of the CPU's Control Register (@ref cp15 c1,c0)
MK_EXTINLINE u32 armGetCp15Cr(void)
{
	u32 reg;
	__asm__ __volatile__ ("mrc p15, 0, %0, c1, c0, 0" : "=r" (reg));
	return reg;
}

//! @brief Sets the CPU's Control Register (@ref cp15 c1,c0)
MK_EXTINLINE void armSetCp15Cr(u32 value)
{
	__asm__ __volatile__ ("mcr p15, 0, %0, c1, c0, 0" :: "r" (value) : "memory");
}

#endif

/*! @brief Temporarily disables interrupts on the CPU
	@return Previous @ref ArmIrqState, to be later passed to @ref armIrqUnlockByPsr
*/
MK_EXTINLINE ArmIrqState armIrqLockByPsr(void)
{
	u32 psr = armGetCpsr();
	armSetCpsrC(psr | ARM_PSR_I | ARM_PSR_F);
	return psr & (ARM_PSR_I | ARM_PSR_F);
}

//! @brief Restores the previous interrupt state @p st locked by @ref armIrqLockByPsr
MK_EXTINLINE void armIrqUnlockByPsr(ArmIrqState st)
{
	u32 psr = armGetCpsr() &~ (ARM_PSR_I | ARM_PSR_F);
	armSetCpsrC(psr | st);
}

#else

MK_EXTERN32 u32 armGetCpsr(void);
MK_EXTERN32 void armSetCpsrC(u32 value);
MK_EXTERN32 u32 armGetSpsr(void);
MK_EXTERN32 void armSetSpsr(u32 value);
MK_EXTERN32 u32 armSwapWord(u32 value, u32* addr);
MK_EXTERN32 u8 armSwapByte(u8 value, u8* addr);

#if __ARM_ARCH >= 5
MK_EXTERN32 void armWaitForIrq(void);
MK_EXTERN32 u32 armGetCp15Cr(void);
MK_EXTERN32 void armSetCp15Cr(u32 value);
#endif

MK_EXTERN32 ArmIrqState armIrqLockByPsr(void);
MK_EXTERN32 void armIrqUnlockByPsr(ArmIrqState st);

#endif

//! @brief Optimized version of memcpy, requiring 32-bit aligned @p dst, @p src and @p size.
MK_EXTERN32 void armCopyMem32(void* dst, const void* src, size_t size);

//! @brief Optimized version of memset, requiring 32-bit aligned @p dst and @p size, and taking a 32-bit fill @p value.
MK_EXTERN32 void armFillMem32(void* dst, u32 value, size_t size);

//! @private
MK_EXTERN32 u32 armContextSave(ArmContext* ctx, ArmIrqState st, u32 ret);

//! @private
MK_EXTERN32 void armContextLoad(const ArmContext* ctx) MK_NORETURN;

MK_EXTERN_C_END

//! @}
