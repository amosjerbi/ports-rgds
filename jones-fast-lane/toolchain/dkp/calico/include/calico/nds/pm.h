// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#if !defined(__NDS__)
#error "This header file is only for NDS"
#endif

#include "../types.h"
#include "io.h"

/*! @addtogroup pm
	@{
*/

/*! @name PM memory mapped I/O
	@{
*/

#if defined(ARM9)

#define REG_POWCNT MK_REG(u32, IO_POWCNT9)

#define POWCNT_LCD         (1U<<0) // supposedly "do not write 0"
#define POWCNT_2D_GFX_A    (1U<<1)
#define POWCNT_3D_RENDER   (1U<<2)
#define POWCNT_3D_GEOMETRY (1U<<3)
#define POWCNT_2D_GFX_B    (1U<<9)
#define POWCNT_ALL         (POWCNT_2D_GFX_A|POWCNT_3D_RENDER|POWCNT_3D_GEOMETRY|POWCNT_2D_GFX_B)
#define POWCNT_LCD_SWAP    (1U<<15)

#elif defined(ARM7)

#define REG_POWCNT MK_REG(u32, IO_POWCNT7)

#define POWCNT_SOUND       (1U<<0)
#define POWCNT_WL_MITSUMI  (1U<<1)
#define POWCNT_ALL         (POWCNT_SOUND|POWCNT_WL_MITSUMI)

#else
#error "ARM9 or ARM7 must be defined"
#endif

//! @}

MK_EXTERN_C_START

//! Power management event
typedef enum PmEvent {
	PmEvent_OnSleep      = 0, //!< The console is about to enter sleep mode
	PmEvent_OnWakeup     = 1, //!< The console is exiting sleep mode
	PmEvent_OnReset      = 2, //!< The application is exiting
} PmEvent;

//! Power LED mode (only meaningful on DS Phat and DS Lite).
typedef enum PmLedMode {
	PmLedMode_Steady    = 0, //!< Steady LED (not blinking)
	PmLedMode_BlinkSlow = 1, //!< Slow blink (sleep mode)
	PmLedMode_BlinkFast = 3, //!< Fast blink (wireless communications)
} PmLedMode;

/*! @brief Event handler callback function
	@param[in] user User provided data passed to @ref pmAddEventHandler
	@param[in] event Event that happened (see @ref PmEvent)
*/
typedef void (* PmEventFn)(void* user, PmEvent event);

// Forward declaration
typedef struct PmEventCookie PmEventCookie;

//! Event handler cookie structure. Each event handler must have one of these.
struct PmEventCookie {
	PmEventCookie* next; //!< @private
	PmEventFn handler;   //!< @private
	void* user;          //!< @private
};

/*! @brief Configures the specified hardware blocks
	@param[in] mask Bitmask of hardware blocks to configure (see POWCNT_\*)
	@param[in] value Bitmask of hardware blocks to enable (0 to disable)
	@note Bits outside POWCNT_ALL are ignored for safety reasons.
*/
MK_INLINE void pmSetControl(unsigned mask, unsigned value)
{
	mask &= POWCNT_ALL;
	REG_POWCNT = (REG_POWCNT &~ mask) | (value & mask);
}

//! Enables the hardware blocks specified by @p mask
MK_INLINE void pmPowerOn(unsigned mask)
{
	pmSetControl(mask, mask);
}

//! Disables the hardware blocks specified by @p mask
MK_INLINE void pmPowerOff(unsigned mask)
{
	pmSetControl(mask, 0);
}

#if defined(ARM9)

//! LCD layout
typedef enum PmLcdLayout {
	PmLcdLayout_BottomIsA = 0, //!< 2D Engine A displays on the bottom screen, B on the bottom screen.
	PmLcdLayout_BottomIsB = 1, //!< 2D Engine B displays on the bottom screen, A on the top screen.
	PmLcdLayout_TopIsB    = PmLcdLayout_BottomIsA,
	PmLcdLayout_TopIsA    = PmLcdLayout_BottomIsB,
} PmLcdLayout;

//! Swaps the LCDs
MK_INLINE void pmGfxLcdSwap(void)
{
	REG_POWCNT ^= POWCNT_LCD_SWAP;
}

//! Configures the specified LCD @p layout
MK_INLINE void pmGfxSetLcdLayout(PmLcdLayout layout)
{
	unsigned reg = REG_POWCNT &~ POWCNT_LCD_SWAP;
	if (layout != PmLcdLayout_BottomIsA) {
		reg |= POWCNT_LCD_SWAP;
	}
	REG_POWCNT = reg;
}

#elif defined(ARM7)

//! Enables or disables the speaker amplifier
void pmSoundSetAmpPower(bool enable);

#endif

//! Microphone amplifier gain preset values
enum {
	PmMicGain_20  = 31,
	PmMicGain_40  = 43,
	PmMicGain_80  = 55,
	PmMicGain_160 = 67,
	PmMicGain_Max = 119,
};

/*! @brief Initializes the power management subsystem
	@note On the ARM9, this is automatically called before main() by default.
	There is no need to call this function.
*/
void pmInit(void);

/*! @brief Registers a power management event handler
	@param[out] cookie Cookie structure owned by the event handler
	@param[in] handler Event handler callback to use
	@param[in] user User-provided parameter to pass to the handler callback
*/
void pmAddEventHandler(PmEventCookie* cookie, PmEventFn handler, void* user);

//! Removes an event handler @p cookie @see pmAddEventHandler
void pmRemoveEventHandler(PmEventCookie* cookie);

/*! @brief Returns true if the application should exit

	There are four situations that cause this to return true:

	- The user pressed the DSi power button (see below)
	- The user pressed the reset key combination (L+R+Start+Select)
	- Either CPU (ARM9 or ARM7) returned from main() or called exit()
	- @ref pmPrepareToReset was called on this CPU

	From a user perspective, the DSi power button can be used in two ways
	depending on how long the button is pressed:

	- Tapping, indicating the intent to exit, usually to the system menu.
	- Holding, indicating the intent to turn off the console.

	Calico will ask the app to exit when the power button begins to be pressed.
	This allows the app to perform any necessary cleanup and save any unsaved data,
	while the system is measuring the duration of the power button press.

	Calico automatically powers off the DSi when the power button is held down,
	even if the app hasn't yet finished its exit processing. This is done in order
	to ensure that the user can easily turn off their console even if the app
	has crashed or is otherwise unresponsive. Under normal circumstances there
	is approximately a delay of 1 second until the power button hold is detected.
	<b>Please make sure that your app can cleanly exit in under 1 second</b>.
*/
bool pmShouldReset(void);

//! Sets a flag that indicates that the application should exit
void pmPrepareToReset(void);

//! Returns true if sleep mode is allowed
bool pmIsSleepAllowed(void);

//! Configures whether sleep mode is @p allowed
void pmSetSleepAllowed(bool allowed);

//! Enters sleep mode
void pmEnterSleep(void);

/*! @brief Returns true if a jump target is configured

	Jump targets allow the application to hand control over to another application upon exiting.
	Calico supports the following jump targets:

	- Return to homebrew menu stub (indicated by the presence of a valid @ref EnvNdsBootstubHeader)
	- Custom jump targets (indicated by `g_envExtraInfo->pm_chainload_flag` being non-zero):
		- If 1, the ARM9/ARM7 entrypoints specified by the @ref EnvNdsHeader at `g_envAppNdsHeader` are reloaded.
		- If 2, the console is switched to GBA mode (only supported on DS Phat and DS Lite).

	Custom jump targets take priority over return to homebrew menu if both are present.

	If no jump targets are configured and the application exits,
	calico will power off (DS mode) or reboot (DSi mode) the console.
*/
bool pmHasResetJumpTarget(void);

//! Removes all configured jump targets @see pmHasResetJumpTarget
void pmClearResetJumpTarget(void);

/*! @brief Processes the current power management state. Call this once in your main loop.
	@return true if the application should continue running, false otherwise (see @ref pmShouldReset)

	This function will automatically handle sleep mode processing when the hinge is closed.

	Example usage:
	@code
	while (pmMainLoop()) {
		threadWaitForVBlank();
		scanKeys();

		// processing etc ...
	}
	@endcode
*/
bool pmMainLoop(void);

//! Bit indicating the console is plugged in (DS Lite and DSi-mode only)
#define PM_BATT_CHARGING  (1U<<7)

/*! @brief Extracts the battery level (0..15).
	@note DS Phat, DS Lite (and DSi/3DS in NDS mode) only expose two battery levels,
	which are rendered as 3 (low) and 15 (high) by calico.
	These values have been chosen to correspond to the equivalent levels on a DSi/3DS in DSi mode.
*/
#define PM_BATT_LEVEL(_x) ((_x)&0x7f)

//! Returns the state of the battery @see PM_BATT_CHARGING, PM_BATT_LEVEL
unsigned pmGetBatteryState(void);

//! Configures the power LED @p mode (only takes effect on DS Phat and DS Lite)
void pmSetPowerLed(PmLedMode mode);

/*! @brief Configures the microphone amplifier
	@param[in] enable Enables or disables the amplifier
	@param[in] gain Microphone gain value (DSi mode: in 0.5 dB units)
	For maximum compatibility between NDS and DSi, use the preset values given by PmMicGain_\*.
*/
void pmMicSetAmp(bool enable, unsigned gain);

/*! @brief Reads data from the DS's built-in firmware/configuration chip
	@param[out] data Data buffer
	@param[in] addr NVRAM byte offset
	@param[in] len Length of the read in bytes
	@return true on success, false on failure
	@warning The buffer **must** be cache line aligned (32-byte aligned).
	Failing to align the buffer **will** cause data corruption on adjacent memory.
*/
bool pmReadNvram(void* data, u32 addr, u32 len);

MK_EXTERN_C_END

//! @}
