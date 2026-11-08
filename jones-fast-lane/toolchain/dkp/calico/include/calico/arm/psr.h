// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once

/*! @addtogroup arm
	@{
*/
/*! @name PSR state flags
	@{
*/

#define ARM_PSR_MODE_USR  0x10  //!< User execution mode
#define ARM_PSR_MODE_FIQ  0x11  //!< FIQ execution mode
#define ARM_PSR_MODE_IRQ  0x12  //!< IRQ execution mode
#define ARM_PSR_MODE_SVC  0x13  //!< Supervisor execution mode
#define ARM_PSR_MODE_ABT  0x17  //!< Data/prefetch abort execution mode
#define ARM_PSR_MODE_UND  0x1b  //!< Undefined instruction execution mode
#define ARM_PSR_MODE_SYS  0x1f  //!< System execution mode
#define ARM_PSR_MODE_MASK 0x1f  //!< Execution mode mask

#define ARM_PSR_T (1<<5)    //!< THUMB execution mode flag
#define ARM_PSR_F (1<<6)    //!< FIQ masked flag
#define ARM_PSR_I (1<<7)    //!< IRQ masked flag
#define ARM_PSR_Q (1<<27)   //!< Q (saturated) condition flag
#define ARM_PSR_V (1<<28)   //!< V (signed overflow) condition flag
#define ARM_PSR_C (1<<29)   //!< C (carry/unsigned overflow) condition flag
#define ARM_PSR_Z (1<<30)   //!< Z (zero) condition flag
#define ARM_PSR_N (1<<31)   //!< N (negative) condition flag

//! @}

//! @}
