// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#if !defined(__NDS__) || !defined(ARM9)
#error "This header file is only for NDS ARM9"
#endif

#include "../mic.h"

/*! @addtogroup mic
	@{
*/

MK_EXTERN_C_START

/*! @brief Microphone recording callback, invoked when an entire audio buffer has been recorded
	@param[in] user User-provided data passed to @ref micSetCallback
	@param[in] buf Address of buffer containing the recorded audio
	@param[in] byte_sz Size in bytes of the recorded audio
*/
typedef void (*MicBufferFn)(void* user, void* buf, size_t byte_sz);

//! Initializes the microphone driver
void micInit(void);

/*! @brief Configures the microphone sampling rate
	@param[in] prescaler CPU timer prescaler (see TIMER_PRESCALER_\*)
	@param[in] period CPU timer period (see @ref timerCalcPeriod)
	@return true on success, false on failure

	Microphone sampling on the Nintendo DS is driven manually using CPU timers.
	This means the ARM7 must execute code to read a sample from the microphone
	at very precise moments. As a result, microphone sampling is very CPU intensive,
	and the performance of other drivers that run on the ARM7 (such as touch
	screen, storage device or wireless networking) will be more affected in
	proportion to the microphone sampling rate.

	On the DSi, the hardware supports entirely automatic (DMA-driven) microphone
	sampling, albeit at only a fixed number of sampling rate presets. Consider
	using @ref micSetDmaRate unless you have a need for a very specific sampling
	rate. Note that @ref micSetDmaRate will graciously fall back to compatible
	CPU timer driven sampling in DS mode.
*/
bool micSetCpuTimer(unsigned prescaler, unsigned period);

/*! @brief Configures the microphone sampling rate using a @p rate preset (see @ref MicRate)
	@return true on success, false on failure

	On the DSi, calico supports using DMA based microphone sampling. This method
	is more efficient because it utilizes hardware resources to automatically fill
	in the buffer without ARM7 CPU involvement; however the hardware only supports
	a fixed number of sampling rates. Unless you have a need for a very specific
	sampling rate, use this function.

	This function falls back to standard CPU timer based sampling in DS mode.
*/
bool micSetDmaRate(MicRate rate);

//! Configures the recording callback @p fn with the specified @p user data
void micSetCallback(MicBufferFn fn, void* user);

/*! @brief Starts microphone recording activity
	@param[out] buf Recording buffer. As an output buffer, it <b>must</b> be cache
	line (32-<b>byte</b>) aligned and visible to ARM7.
	@param[in] byte_sz Size of the buffer in bytes (must be 32-<b>byte</b> aligned)
	@param[in] fmt Recording format (see @ref MicFmt)
	@param[in] mode Recording mode (see @ref MicMode)
	@return true on success, false on failure

	@note For @ref MicMode_DoubleBuffer, @p buf points to the first buffer, and @p byte_sz
	specifies the size of a single buffer. The second buffer starts at `(u8*)buf + byte_sz`.

	@note Do not forget to call @ref pmMicSetAmp in order to turn on/off the microphone amplifier!
*/
bool micStart(void* buf, size_t byte_sz, MicFmt fmt, MicMode mode);

//! Stops microphone recording activity
void micStop(void);

MK_EXTERN_C_END

//! @}
