// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#if !defined(__NDS__) || !defined(ARM7)
#error "This header file is only for NDS ARM7"
#endif

#include "../../types.h"
#include "spi.h"
#include "tsc.h"

MK_EXTERN_C_START

typedef enum CdcPage {
	CdcPage_Control    = 0x00,
	CdcPage_Sound      = 0x01,
	CdcPage_TscControl = 0x03,
	CdcPage_AdcCoeffs  = 0x04, // 0x04..0x05
	CdcPage_DacCoeffsA = 0x08, // 0x08..0x0b
	CdcPage_DacCoeffsB = 0x0c, // 0x0c..0x0f
	CdcPage_AdcInstr   = 0x20, // 0x20..0x2b
	CdcPage_DacInstr   = 0x40, // 0x40..0x5f
	CdcPage_Dummy      = 0x63,
	CdcPage_TscData    = 0xfc,
	CdcPage_DsMode     = 0xff, // Nintendo custom
} CdcPage;

typedef enum CdcTscCtrlReg {
	CdcTscCtrlReg_SarAdcCtrl     = 0x02,
	CdcTscCtrlReg_SarAdcConvMode = 0x03,
	CdcTscCtrlReg_PrechargeSense = 0x04,
	CdcTscCtrlReg_PanelVoltStblz = 0x05,
	CdcTscCtrlReg_Status0        = 0x09,
	CdcTscCtrlReg_BufferMode     = 0x0e, // Normally 0x0d. Not exactly following datasheet
	CdcTscCtrlReg_ScanModeTimer  = 0x0f,
	CdcTscCtrlReg_DebounceTimer  = 0x12,
	CdcTscCtrlReg_DacDataPath    = 0x3f,
	CdcTscCtrlReg_AdcDigitalMic  = 0x51,
	CdcTscCtrlReg_AdcDigitalVolFine = 0x52,
} CdcTscCtrlReg;

typedef enum CdcTscSndReg {
	CdcTscSndReg_MicBias         = 0x2e,
	CdcTscSndReg_MicPga          = 0x2f,
} CdcTscSndReg;

MK_INLINE bool cdcIsTwlMode(void)
{
	extern bool g_cdcIsTwlMode;
	return g_cdcIsTwlMode;
}

u8 cdcReadReg(CdcPage page, unsigned reg);
bool cdcWriteReg(CdcPage page, unsigned reg, u8 value);
bool cdcWriteRegMask(CdcPage page, unsigned reg, u8 mask, u8 value);

bool cdcReadRegArray(CdcPage page, unsigned reg, void* data, unsigned len);
bool cdcWriteRegArray(CdcPage page, unsigned reg, const void* data, unsigned len);

void cdcTscInit(void);
TscResult cdcTscReadTouch(TscTouchData* out, unsigned diff_threshold, u16* out_max_diff);

void cdcMicSetAmp(bool enable, unsigned gain);

MK_EXTERN_C_END
