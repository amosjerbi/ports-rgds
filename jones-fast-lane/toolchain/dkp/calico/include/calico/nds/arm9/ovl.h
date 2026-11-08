// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#include "../../types.h"
#include "../env.h"

/*! @addtogroup ovl

	Overlays allow large programs to be split up in smaller pieces that can be
	loaded dynamically. Moreover, pieces of code that do not need to be accessed
	simultaneously can be loaded to the same address, therefore saving memory.
	An example would be code related to different game modes (e.g. overworld/battle),
	menu systems, or stage specific effects.

	The DS ROM file format has support for overlay files that are loaded to
	predetermined memory addresses. In order to add overlays to the executable,
	it is necessary to use a custom linker script that places the desired
	code/data sections into special overlay segments that are then processed by
	ndstool and converted into overlay files.

	For a simple example of how to use overlays, see nds-examples/filesystem/nitrofs/overlays.

	@{
*/

MK_EXTERN_C_START

typedef EnvNdsOverlay OvlParams;   //!< @private
typedef void (*OvlStaticFn)(void); //!< @private

/*! @brief Initializes the overlay system

	The overlay system uses @ref nitroromGetSelf to obtain access to the
	application's DS ROM. Please refer to its documentation for more details.

	@return true on success, false on failure
*/
bool ovlInit(void);

/*! @brief Preloads the specified overlay into its target address, without activating it
	@param[in] ovl_id ID of the overlay to load
	@return true on success, false on failure
*/
bool ovlLoadInPlace(unsigned ovl_id);

//! Activates overlay @p ovl_id, invoking its static constructors
void ovlActivate(unsigned ovl_id);

//! Deactivates overlay @p ovl_id, invoking its static destructors
void ovlDeactivate(unsigned ovl_id);

/*! @brief Loads and activates the specified overlay
	@param[in] ovl_id ID of the overlay to load
	@return true on success, false on failure
*/
MK_INLINE bool ovlLoadAndActivate(unsigned ovl_id)
{
	bool ok = ovlLoadInPlace(ovl_id);
	if (ok) {
		ovlActivate(ovl_id);
	}
	return ok;
}

MK_EXTERN_C_END

//! @}
