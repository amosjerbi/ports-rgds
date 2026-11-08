// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#if !defined(__NDS__) || !defined(ARM7)
#error "This header file is only for NDS ARM7"
#endif

#include "../../types.h"
#include "spi.h"

#define NVRAM_PAGE_SZ 0x100

#define NVRAM_STATUS_WIP (1U<<0) // Write In Progress
#define NVRAM_STATUS_WEL (1U<<1) // Write Enable Latch

MK_EXTERN_C_START

typedef enum NvramCmd {
	NvramCmd_WriteEnable       = 0x06,
	NvramCmd_WriteDisable      = 0x04,
	NvramCmd_ReadJedec         = 0x9f,
	NvramCmd_ReadStatus        = 0x05,
	NvramCmd_ReadDataBytes     = 0x03,
	NvramCmd_ReadDataBytesFast = 0x0b,
	NvramCmd_PageWrite         = 0x0a,
	NvramCmd_PageProgram       = 0x02,
	NvramCmd_PageErase         = 0xdb,
	NvramCmd_SectorErase       = 0xd8,
	NvramCmd_EnterDeepSleep    = 0xb9,
	NvramCmd_LeaveDeepSleep    = 0xab,
} NvramCmd;

bool nvramWaitReady(void);
bool nvramReadJedec(u32* out);
bool nvramReadDataBytes(void* data, u32 addr, u32 len);

MK_EXTERN_C_END
