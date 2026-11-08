// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#if !defined(__NDS__)
#error "This header file is only for NDS"
#endif

#include "../types.h"
#include "../system/sysclock.h"
#include "timer.h"

/*! @addtogroup mic
	@{
*/

//! Microphone sampling formats
typedef enum MicFmt {
	MicFmt_Pcm8  = 0, //!< Signed 8-bit PCM
	MicFmt_Pcm16 = 1, //!< Signed 16-bit PCM
} MicFmt;

//! Microphone sampling modes
typedef enum MicMode {
	MicMode_OneShot      = 0, //!< Records a single audio buffer, and stops afterwards
	MicMode_Repeat       = 1, //!< Records a single audio buffer continuously
	MicMode_DoubleBuffer = 2, //!< Records audio to two alternating and consecutive buffers
} MicMode;

//! Microphone sampling rate presets
typedef enum MicRate {
	MicRate_Full = 0, //!< Sampling frequency is equal to sound mixer output (typically 32728 Hz, see @ref SOUND_MIXER_FREQ_HZ)
	MicRate_Div2 = 1, //!< Sampling frequency is 1/2 of sound mixer output (typically 16364 Hz)
	MicRate_Div3 = 2, //!< Sampling frequency is 1/3 of sound mixer output (typically 10909 Hz)
	MicRate_Div4 = 3, //!< Sampling frequency is 1/4 of sound mixer output (typically 8182 Hz)
} MicRate;

//! Calculates the required sound timer value for the specified mic @p rate
MK_CONSTEXPR unsigned soundTimerFromMicRate(MicRate rate)
{
	return 512*((unsigned)rate + 1);
}

//! @}
