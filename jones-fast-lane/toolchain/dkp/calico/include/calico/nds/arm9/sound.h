// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#if !defined(__NDS__) || !defined(ARM9)
#error "This header file is only for NDS ARM9"
#endif

#include "../sound.h"

/*! @addtogroup sound

	The sound hardware of the NDS is connected to the ARM7, and cannot be accessed
	directly by the ARM9. In order to solve this problem, calico runs a sound driver
	on the ARM7, and provides a command interface to communicate with it on the ARM9.
	Sound commands are sent asynchronously to the sound driver. However, if the ARM7
	runs out of space for queued commands the ARM9 will wait until enough space is
	available. In addition, the sound driver provides feedback on the current state
	of sound channels. This state can be continuously updated using @ref soundSetAutoUpdate,
	or manually updated using @ref soundSynchronize.

	This sound driver offers low level access to all features provided by the NDS sound
	hardware. While it is possible to implement a complex music and sound effect engine
	on the ARM9 with this API, a dedicated engine such as Maxmod that directly runs on
	the ARM7 is more efficient and should be used instead.

	@{
*/

//! Bitwise-or this value to the `ch` parameter of @ref soundPreparePcm or @ref soundPreparePsg to immediately start the channel
#define SOUND_START (1U<<4)

MK_EXTERN_C_START

//! Initializes the ARM9 interface to the sound driver. Call this before using sound functions
void soundInit(void);

/*! @brief Synchronizes with the sound driver, and updates the sound channel state.

	This function will block the ARM9 until the driver has finished processing
	all commands up to and including this one.
	If auto-update is on (see @ref soundSetAutoUpdate) it is not necessary
	to call this function prior to reading the sound channel state (@ref soundGetActiveChannels)
*/
void soundSynchronize(void);

//! Sets the power state of the sound hardware
void soundSetPower(bool enable);

/*! @brief Enables or disables the auto-update mechanism of the sound driver.

	When auto-update is on, the sound driver will periodically update the
	sound channel state (@ref soundGetActiveChannels).
	The frequency of updates is given by @ref SOUND_UPDATE_HZ.

	By default, auto-update is off in order to reduce ARM7 CPU load.
*/
void soundSetAutoUpdate(bool enable);

//! Powers on the sound hardware
MK_INLINE void soundPowerOn(void)
{
	soundSetPower(true);
}

//! Powers off the sound hardware
MK_INLINE void soundPowerOff(void)
{
	soundSetPower(false);
}

/*! @brief Returns a bitmask of sound channels currently playing audio
	@note If auto-update is disabled, this function will implicitly
	call @ref soundSynchronize in order to ensure the correct data is returned.
	If you intend to call this function often, consider enabling auto-update
	(see @ref soundSetAutoUpdate).
*/
unsigned soundGetActiveChannels(void);

//! Sets the main volume of the sound mixer to @p vol (0..127)
void soundSetMixerVolume(unsigned vol);

//! @private
void soundSetMixerConfigDirect(unsigned config);

/*! @brief Configures whether sleep mode powers off the sound mixer

	By default, the sound driver powers off the sound mixer when entering
	sleep mode in order to conserve battery life. This has the effect of
	restarting sound channel playback from the beginning after the console
	wakes up from sleep mode. This behavior is sometimes undesirable, such
	as when implementing audio streaming. Disabling mixer sleep preserves
	the playback state of sound channels during sleep mode, albeit at the
	cost of theoretically increasing power consumption. Consider registering
	a power management callback instead (see @ref pmAddEventHandler), in
	order to gracefully handle interactions between sleep mode and audio.
*/
void soundSetMixerSleep(bool enable);

/*! @brief Configures sound channel @p ch to play back a PCM audio sample
	@param[in] ch Sound channel number (0..15)
	@param[in] vol 11-bit volume (0..2047)
	@param[in] pan 7-bit pan (0..127), where 64 is center
	@param[in] timer Timer value (see @ref soundTimerFromHz)
	@param[in] mode Playback mode (see @ref SoundMode)
	@param[in] fmt Sample format (see @ref SoundFmt)
	@param[in] sad Source address of the sample (must be 32-bit aligned and visible by ARM7)
	@param[in] pnt Loop point in words
	@param[in] len Sample length in words
	@note It is possible to simultaneously prepare and start the channel by passing `value|SOUND_START` to @p ch
*/
void soundPreparePcm(
	unsigned ch, unsigned vol, unsigned pan, unsigned timer,
	SoundMode mode, SoundFmt fmt, const void* sad, unsigned pnt, unsigned len);

/*! @brief Configures sound channel @p ch to play back a PSG tone
	@param[in] ch PSG-compatible sound channel number (8..13 = square wave, 14..15 = noise)
	@param[in] vol 11-bit volume (0..2047)
	@param[in] pan 7-bit pan (0..127), where 64 is center
	@param[in] timer Timer value (see @ref soundTimerFromHz)
	@param[in] duty Duty cycle of square wave channels (see @ref SoundDuty), ignored for noise channels
	@note It is possible to simultaneously prepare and start the channel by passing `value|SOUND_START` to @p ch
	@note The generated square waves have the equivalent of 8 samples. In order to play a tone with
	frequency F, use `soundTimerFromHz(8*F)`.
*/
void soundPreparePsg(unsigned ch, unsigned vol, unsigned pan, unsigned timer, SoundDuty duty);

//! @private
void soundPrepareCapDirect(unsigned cap, unsigned config, void* dad, unsigned len);

/*! @brief Starts sound channels and/or capture units
	@param[in] mask Bit 0..15 correspond to sound channels, whereas bit 16..17 correspond to capture units
*/
void soundStart(u32 mask);

//! Stops sound channels and/or capture units (see @ref soundStart)
void soundStop(u32 mask);

//! Sets the volume of sound channel @p ch to @p vol (0..2047)
void soundChSetVolume(unsigned ch, unsigned vol);

//! Sets the pan of sound channel @p ch to @p pan (0..127), where 64 is center
void soundChSetPan(unsigned ch, unsigned pan);

//! Sets the sound timer of sound channel @p ch to @p timer (see @ref soundTimerFromHz)
void soundChSetTimer(unsigned ch, unsigned timer);

//! Sets the PSG square wave duty cycle of sound channel @p ch to @p duty (see @ref SoundDuty)
void soundChSetDuty(unsigned ch, SoundDuty duty);

/*! @brief Configures the sound mixer
	@param[in] src_l Source of the left speaker output (see @ref SoundOutSrc)
	@param[in] src_r Source of the right speaker output (see @ref SoundOutSrc)
	@param[in] mute_ch1 Pass true to mute sound channel 1 (i.e. output channel of capture unit 0)
	@param[in] mute_ch3 Pass true to mute sound channel 3 (i.e. output channel of capture unit 1)
*/
MK_INLINE void soundSetMixerConfig(SoundOutSrc src_l, SoundOutSrc src_r, bool mute_ch1, bool mute_ch3)
{
	unsigned config = soundMakeMixerConfig(src_l, src_r, mute_ch1, mute_ch3);
	soundSetMixerConfigDirect(config);
}

/*! @brief Configures capture unit @p cap
	@param[in] cap Capture unit number (0..1)
	@param[out] dst Output channel destination (see @ref SoundCapDst)
	@param[in] src Capture source (see @ref SoundCapSrc)
	@param[in] loop Pass true to perform a looped capture
	@param[in] fmt Capture sample format (see @ref SoundCapFmt)
	@param[in] dad Destination address of the capture buffer (must be 32-bit aligned and visible by ARM7)
	@param[in] len Length of the capture buffer in words

	Each capture unit has two associated sound mixer channels:
	- Unit 0 is associated to channels 0 (input) and 2 (output)
	- Unit 1 is associated to channels 1 (input) and 3 (output)

	Audio is captured using the timer configured by its associated output channel.
	This output channel is intended to be used to play back the captured audio,
	therefore creating the wet part of a reverb effect.
*/
MK_INLINE void soundPrepareCap(
	unsigned cap, SoundCapDst dst, SoundCapSrc src, bool loop, SoundCapFmt fmt,
	void* dad, unsigned len)
{
	unsigned config = soundMakeCapConfig(dst, src, loop, fmt);
	soundPrepareCapDirect(cap, config, dad, len);
}

MK_EXTERN_C_END

//! @}
