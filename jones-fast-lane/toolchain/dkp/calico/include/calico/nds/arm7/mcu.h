// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#if !defined(__NDS__) || !defined(ARM7)
#error "This header file is only for NDS ARM7"
#endif

#include "../../types.h"

#define MCU_IRQ_PWRBTN_RESET    (1U << 0) // Depressed prior to a period of time = Reset request
#define MCU_IRQ_PWRBTN_SHUTDOWN (1U << 1) // Held down for a period of time = Shutdown request
#define MCU_IRQ_PWRBTN_BEGIN    (1U << 3) // Begin pressing
#define MCU_IRQ_BATTERY_EMPTY   (1U << 4)
#define MCU_IRQ_BATTERY_LOW     (1U << 5)
#define MCU_IRQ_VOLBTN          (1U << 6)

MK_EXTERN_C_START

typedef enum McuRegister {
	McuReg_Version  = 0x00,
	McuReg_Unk01    = 0x01,
	McuReg_Unk02    = 0x02,

	McuReg_IrqFlags = 0x10,
	McuReg_DoReset  = 0x11,
	McuReg_Config   = 0x12,

	McuReg_BatteryState = 0x20,
	McuReg_BatteryUnk   = 0x21,

	McuReg_WifiLed = 0x30,
	McuReg_CamLed  = 0x31,

	McuReg_VolumeLevel    = 0x40,
	McuReg_BacklightLevel = 0x41,

	McuReg_Unk60    = 0x60,
	McuReg_Unk61    = 0x61,
	McuReg_Unk62    = 0x62,
	McuReg_PowerLed = 0x63,

	McuReg_User0 = 0x70,
	McuReg_User1 = 0x71,
	McuReg_User2 = 0x72,
	McuReg_User3 = 0x73,
	McuReg_User4 = 0x74,
	McuReg_User5 = 0x75,
	McuReg_User6 = 0x76,
	McuReg_User7 = 0x77,

	McuReg_WarmbootFlag = McuReg_User0,

	McuReg_PwrBtnTapDelay  = 0x80,
	McuReg_PwrBtnHoldDelay = 0x81,
} McuRegister;

typedef enum McuPwrBtnState {
	McuPwrBtnState_Normal   = 0,
	McuPwrBtnState_Begin    = 1,
	McuPwrBtnState_Reset    = 2,
	McuPwrBtnState_Shutdown = 3,
} McuPwrBtnState;

typedef void (*McuIrqHandler)(unsigned irq_mask);

MK_INLINE McuPwrBtnState mcuGetPwrBtnState(void)
{
	extern McuPwrBtnState g_mcuPwrBtnState;
	return g_mcuPwrBtnState;
}

void mcuInit(void);
void mcuStartThread(u8 thread_prio);
void mcuIrqSet(unsigned irq_mask, McuIrqHandler fn);
void mcuIssueReset(void) MK_NORETURN;
void mcuIssueShutdown(void) MK_NORETURN;

MK_EXTERN_C_END
