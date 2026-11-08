// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#if !defined(__NDS__) || !defined(ARM7)
#error "This header file is only for NDS ARM7"
#endif

#include "../../types.h"
#include "spi.h"

// Bits for PmicReg_Control
#define PMIC_CTRL_SOUND_ENABLE   (1U<<0)
#define PMIC_CTRL_SOUND_MUTE     (1U<<1)
#define PMIC_CTRL_LCD_BL_BOTTOM  (1U<<2)
#define PMIC_CTRL_LCD_BL_TOP     (1U<<3)
#define PMIC_CTRL_LED_STEADY     (0U<<4)
#define PMIC_CTRL_LED_BLINK_SLOW (1U<<4)
#define PMIC_CTRL_LED_BLINK_FAST (3U<<4)
#define PMIC_CTRL_LED_MASK       (3U<<4)
#define PMIC_CTRL_SHUTDOWN       (1U<<6)

// Bits for PmicReg_BatteryStatus
#define PMIC_BATT_STAT_NORMAL    (0U<<0)
#define PMIC_BATT_STAT_LOW       (1U<<0)

// Bits for PmicReg_MicAmpControl
#define PMIC_MIC_AMP_CTRL_ENABLE (1U<<0)

// Bits for PmicReg_MicAmpGain
#define PMIC_MIC_AMP_GAIN(_x)    ((_x)&3)
#define PMIC_MIC_AMP_GAIN_MASK   (3U<<0)

// Bits for PmicReg_BacklightLevel
#define PMIC_BL_LEVEL(_x)        ((_x)&3)
#define PMIC_BL_LEVEL_MASK       (3U<<0)
#define PMIC_BL_CHARGER_BL_MAX   (1U<<2)
#define PMIC_BL_CHARGER_DETECTED (1U<<3)

// Bits for PmicReg_ControlExt
#define PMIC_CTRL_EXT_RESET      (1U<<0)
#define PMIC_CTRL_EXT_USER       (1U<<1)

MK_EXTERN_C_START

typedef enum PmicRegister {
	PmicReg_Control        = 0x00,
	PmicReg_BatteryStatus  = 0x01,
	PmicReg_MicAmpControl  = 0x02,
	PmicReg_MicAmpGain     = 0x03,
	PmicReg_BacklightLevel = 0x04, // DS Lite exclusive
	PmicReg_ControlExt     = 0x10, // DSi exclusive
} PmicRegister;

typedef enum PmicMicGain {
	PmicMicGain_20  = 0,
	PmicMicGain_40  = 1,
	PmicMicGain_80  = 2,
	PmicMicGain_160 = 3,
} PmicMicGain;

bool pmicWriteRegister(PmicRegister reg, u8 data);
u8 pmicReadRegister(PmicRegister reg);

void pmicIssueShutdown(void) MK_NORETURN;

MK_EXTERN_C_END
