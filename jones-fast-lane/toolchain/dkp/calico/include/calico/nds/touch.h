// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#if !defined(__NDS__)
#error "This header file is only for NDS"
#endif

#include "../types.h"

/*! @addtogroup touch
	@{
*/

MK_EXTERN_C_START

//! Touchscreen state
typedef struct MK_STRUCT_ALIGN(4) TouchData {
	u16 px;   //!< Touch X position in pixels
	u16 py;   //!< Touch Y position in pixels
	u16 rawx; //!< Raw 12-bit touch X position reported by the ADC
	u16 rawy; //!< Raw 12-bit touch Y position reported by the ADC
} TouchData;

#if defined(ARM7)
void touchInit(void);
void touchStartServer(unsigned lyc, u8 thread_prio);
void touchLoadCalibration(void);
#endif

/*! @brief Obtains the current state of the touchscreen
	@param[out] out @ref TouchData structure to fill
	@return true if the screen is being touched, false otherwise
*/
bool touchRead(TouchData* out);

MK_EXTERN_C_END

//! @}
