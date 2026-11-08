// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#if !defined(__NDS__) || !defined(ARM7)
#error "This header file is only for NDS ARM7"
#endif

#include "../../types.h"

MK_EXTERN_C_START

typedef enum DebugBufferMode {
	DbgBufMode_None = 0,
	DbgBufMode_Line = 1,
	DbgBufMode_Full = 2,
} DebugBufferMode;

MK_INLINE void debugSetBufferMode(DebugBufferMode mode) {
	extern DebugBufferMode g_dbgBufMode;
	g_dbgBufMode = mode;
}

void debugOutput(const char* buf, size_t size);

MK_EXTERN_C_END
