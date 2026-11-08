// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#if !defined(__NDS__)
#error "This header file is only for NDS"
#endif

#include "../types.h"
#include "../system/sysclock.h"

/*! @addtogroup sound
	For more details on the DS sound hardware, see https://problemkaputt.de/gbatek-ds-sound.htm
	@{
*/

//! Base frequency used by sound channel timers
#define SOUND_CLOCK         (SYSTEM_CLOCK/2)
//! Output frequency of the sound mixer, usually ~32728.5 Hz
#define SOUND_MIXER_FREQ_HZ (SYSTEM_CLOCK/1024)
//! Update frequency of the sound driver
#define SOUND_UPDATE_HZ     192
//! Total number of sound channels in the mixer
#define SOUND_NUM_CHANNELS  16
//! Total number of sound capture units
#define SOUND_NUM_CAPTURES  2

MK_EXTERN_C_START

//! Speaker output sources
typedef enum SoundOutSrc {
	SoundOutSrc_Mixer  = 0, //!< Left/right mixer output
	SoundOutSrc_Ch1    = 1, //!< Sound mixer channel 1 (i.e. output channel of capture unit 0)
	SoundOutSrc_Ch3    = 2, //!< Sound mixer channel 3 (i.e. output channel of capture unit 1)
	SoundOutSrc_Ch1Ch3 = 3, //!< Addition of sound mixer channels 1 and 3
} SoundOutSrc;

//! Sound channel volume divisors
typedef enum SoundVolDiv {
	SoundVolDiv_1  = 0,
	SoundVolDiv_2  = 1,
	SoundVolDiv_4  = 2,
	SoundVolDiv_16 = 3,
} SoundVolDiv;

//! Sound channel playback modes
typedef enum SoundMode {
	SoundMode_Manual  = 0, //!< Plays back sample data without any automatic looping/length control
	SoundMode_Repeat  = 1, //!< Loops the sample using the specified loop point and length, until the channel is manually stopped
	SoundMode_OneShot = 2, //!< Plays back the sample once using the specified length, and afterwards stops the channel
} SoundMode;

//! Sound channel sample formats
typedef enum SoundFmt {
	SoundFmt_Pcm8     = 0, //!< Signed 8-bit PCM
	SoundFmt_Pcm16    = 1, //!< Signed 16-bit PCM
	SoundFmt_ImaAdpcm = 2, //!< IMA-ADPCM (4-bit)
	SoundFmt_Psg      = 3, //!< Special value used by PSG-compatible sound channels
} SoundFmt;

//! Duty cycle of PSG square wave channels
typedef enum SoundDuty {
	SoundDuty_12_5 = 0, //!< 12.5% high, 87.5% low (`_-------`)
	SoundDuty_25   = 1, //!< 25%   high, 75%   low (`__------`)
	SoundDuty_37_5 = 2, //!< 37.5% high, 62.5% low (`___-----`)
	SoundDuty_50   = 3, //!< 50%   high, 50%   low (`____----`)
	SoundDuty_62_5 = 4, //!< 62.5% high, 37.5% low (`_____---`)
	SoundDuty_75   = 5, //!< 75%   high, 25%   low (`______--`)
	SoundDuty_87_5 = 6, //!< 87.5% high, 12.5% low (`_______-`)
	SoundDuty_0    = 7, //!< 0%    high, 100%  low (`________`)
} SoundDuty;

//! Sound capture unit output channel destinations
typedef enum SoundCapDst {
	// Generic
	SoundCapDst_ChBNormal = 0, //!< Outputs the output channel normally
	SoundCapDst_ChBAddToA = 1, //!< Adds the output channel to the input channel

	// For capture 0
	SoundCapDst_Ch1Normal = SoundCapDst_ChBNormal, //!< (Unit 0) Outputs mixer channel 1 normally
	SoundCapDst_Ch1AddTo0 = SoundCapDst_ChBAddToA, //!< (Unit 0) Adds mixer channel 1 to channel 0

	// For capture 1
	SoundCapDst_Ch3Normal = SoundCapDst_ChBNormal, //!< (Unit 1) Outputs mixer channel 3 normally
	SoundCapDst_Ch3AddTo2 = SoundCapDst_ChBAddToA, //!< (Unit 1) Adds mixer channel 3 to channel 2
} SoundCapDst;

//! Sound capture unit sources
typedef enum SoundCapSrc {
	// Generic
	SoundCapSrc_Mixer      = 0, //!< Captures audio from the left/right mixer output
	SoundCapSrc_ChA        = 1, //!< Captures audio from the associated input channel

	// For capture 0
	SoundCapSrc_LeftMixer  = SoundCapSrc_Mixer, //!< (Unit 0) Captures audio from the left mixer output
	SoundCapSrc_Ch0        = SoundCapSrc_ChA,   //!< (Unit 0) Captures audio from mixer channel 0

	// For capture 1
	SoundCapSrc_RightMixer = SoundCapSrc_Mixer, //!< (Unit 1) Captures audio from the right mixer output
	SoundCapSrc_Ch2        = SoundCapSrc_ChA,   //!< (Unit 1) Captures audio from mixer channel 2
} SoundCapSrc;

//! Sound capture unit sample formats
typedef enum SoundCapFmt {
	SoundCapFmt_Pcm16 = 0, //!< Signed 16-bit PCM
	SoundCapFmt_Pcm8  = 1, //!< Signed 8-bit PCM
} SoundCapFmt;

//! Calculates the sound timer period value for a given frequency in @p hz
MK_CONSTEXPR unsigned soundTimerFromHz(unsigned hz)
{
	return (SOUND_CLOCK + hz/2) / hz;
}

//! @private
MK_CONSTEXPR unsigned soundMakeMixerConfig(SoundOutSrc src_l, SoundOutSrc src_r, bool mute_ch1, bool mute_ch3)
{
	return (src_l&3) | ((src_r&3)<<2) | (mute_ch1<<4) | (mute_ch3<<5);
}

//! @private
MK_CONSTEXPR unsigned soundMakeCapConfig(SoundCapDst dst, SoundCapSrc src, bool loop, SoundCapFmt fmt)
{
	return (dst&1) | ((src&1)<<1) | ((!loop)<<2) | ((fmt&1)<<3);
}

MK_EXTERN_C_END

//! @}
