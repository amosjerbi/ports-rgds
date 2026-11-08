// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once

/*! @addtogroup system
	@{
*/

#if defined(__GBA__)
#define SYSTEM_CLOCK 0x1000000 //!< GBA system bus speed: exactly 2^24 Hz ~= 16.78 MHz
#elif defined(__NDS__)
#define SYSTEM_CLOCK 0x1FF61FE //!< NDS system bus speed: approximately 33.51 MHz
#else
#error "This header file is only for GBA and NDS"
#endif

//! @}
