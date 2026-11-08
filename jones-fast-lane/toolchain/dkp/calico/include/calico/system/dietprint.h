// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#include <stdarg.h>
#include "../types.h"

/*! @addtogroup system
	@{
*/

/*! @name Lightweight print formatting

	Calico provides a lightweight alternative to printf for use in memory
	constrained scenarios such as the ARM7. @ref dietPrint supports all
	features of standard C format specifier syntax, with the following
	exceptions:

	- `%%o` (octal) and `%%n` (number of characters written) are not supported.
	- `%%ls` and `%%lc` ("wide" strings and characters) are not supported.
	- Floating point arguments are not supported.
	- Decimal 64-bit integers larger than `UINT32_MAX` are not supported.
	- POSIX-style argument reordering (e.g. `%1$s`) is not supported.

	@{
*/

MK_EXTERN_C_START

/*! @brief Printing callback used by @ref dietPrint.
	@note The incoming @p buf may be NULL, in which case the callback is
	expected to output the number of space characters given by @p size.
*/
typedef void (*DietPrintFn)(const char* buf, size_t size);

/*! @brief Sets @p fn as the current printing callback.
	@warning This function changes global state, and as such is not thread safe.
*/
MK_INLINE void dietPrintSetFunc(DietPrintFn fn)
{
	extern DietPrintFn g_dietPrintFn;
	g_dietPrintFn = fn;
}

//! @brief Prints the specified formatted text (works like printf).
void dietPrint(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

//! @brief Prints the specified formatted text (works like vprintf).
void dietPrintV(const char* fmt, va_list va) __attribute__((format(printf, 1, 0)));

MK_EXTERN_C_END

//! @}

//! @}
