// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#if !defined(__NDS__) || !defined(ARM9)
#error "This header file is only for NDS ARM9"
#endif

#include "../../types.h"

/*! @addtogroup env
	@{
*/

MK_EXTERN_C_START

/*! @brief ARM7 debug output callback
	@param[in] buf Message buffer (not NUL-terminated)
	@param[in] size Length of the message buffer in bytes
*/
typedef void (* Arm7DebugFn)(const char* buf, size_t size);

/*! @brief Starts a server thread for handling the ARM7's debug output
	@param[in] fn Callback function (see @ref Arm7DebugFn)
	@param[in] thread_prio Priority of the server thread
*/
void installArm7DebugSupport(Arm7DebugFn fn, u8 thread_prio);

MK_EXTERN_C_END

//! @}
