// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#if !defined(__NDS__) || !defined(ARM7)
#error "This header file is only for NDS ARM7"
#endif

#include "../../types.h"
#include "spi.h"

MK_EXTERN_C_START

typedef enum TscPowerMode {
	TscPowerMode_Auto  = 0U<<0,
	TscPowerMode_AdcOn = 1U<<0,
	TscPowerMode_RefOn = 2U<<0,
	TscPowerMode_AllOn = 3U<<0,
} TscPowerMode;

typedef enum TscRef {
	TscRef_Differential = 0U<<2,
	TscRef_SingleEnded  = 1U<<2,
} TscRef;

typedef enum TscConvMode {
	TscConvMode_12bit = 0U<<3,
	TscConvMode_8bit  = 1U<<3,
} TscConvMode;

typedef enum TscChannel {
	TscChannel_T0   = (0U<<4) | TscRef_SingleEnded,
	TscChannel_Y    = (1U<<4) | TscRef_Differential,
	TscChannel_VBAT = (2U<<4) | TscRef_SingleEnded,
	TscChannel_Z1   = (3U<<4) | TscRef_Differential,
	TscChannel_Z2   = (4U<<4) | TscRef_Differential,
	TscChannel_X    = (5U<<4) | TscRef_Differential,
	TscChannel_AUX  = (6U<<4) | TscRef_SingleEnded,
	TscChannel_T1   = (7U<<4) | TscRef_SingleEnded,
} TscChannel;

typedef enum TscResult {
	TscResult_None  = 0,
	TscResult_Noisy = 1,
	TscResult_Valid = 2,
} TscResult;

typedef struct MK_STRUCT_ALIGN(4) TscTouchData {
	u16 x, y;
} TscTouchData;

MK_CONSTEXPR unsigned tscAbs(signed x)
{
	return x >= 0 ? x : (-x);
}

MK_CONSTEXPR u8 tscMakeCmd(TscChannel ch, TscConvMode conv, TscPowerMode pm)
{
	return pm | conv | ch | (1U<<7);
}

void tscInit(void);
TscResult tscReadTouch(TscTouchData* out, unsigned diff_threshold, u16* out_max_diff);
unsigned tscReadChannel8(TscChannel ch);
unsigned tscReadChannel12(TscChannel ch);

MK_EXTERN_C_END
