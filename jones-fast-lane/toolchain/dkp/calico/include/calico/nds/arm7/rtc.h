// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#if !defined(__NDS__) || !defined(ARM7)
#error "This header file is only for NDS ARM7"
#endif

#include "../../types.h"

#define RTC_STATUS1_RESET (1U<<0)
#define RTC_STATUS1_12HRS (0U<<1)
#define RTC_STATUS1_24HRS (1U<<1)
#define RTC_STATUS1_USER0 (1U<<2)
#define RTC_STATUS1_USER1 (1U<<3)
#define RTC_STATUS1_INT1  (1U<<4)
#define RTC_STATUS1_INT2  (1U<<5)
#define RTC_STATUS1_BLD   (1U<<6)
#define RTC_STATUS1_POC   (1U<<7)

#define RTC_STATUS2_INT1_MODE(_x) ((_x)&0xf)
#define RTC_STATUS2_USER2         (1U<<4)
#define RTC_STATUS2_USER3         (1U<<5)
#define RTC_STATUS2_INT2_EN       (1U<<6)
#define RTC_STATUS2_TEST          (1U<<7)

MK_EXTERN_C_START

typedef enum RtcRegister {
	// Available on all models (Seiko S-35180)
	RtcReg_Status1    = 0x0,
	RtcReg_Status2    = 0x1,
	RtcReg_DateTime   = 0x2,
	RtcReg_Time       = 0x3,
	RtcReg_FreqDuty   = 0x4,
	RtcReg_Alarm1Time = 0x4,
	RtcReg_Alarm2Time = 0x5,
	RtcReg_ClockAdj   = 0x6,
	RtcReg_User       = 0x7,

	// Available on DSi (Seiko S-35199A01)
	RtcReg_UpCounter  = 0x8,
	RtcReg_FoutHi     = 0x9,
	RtcReg_FoutLo     = 0xa,
	RtcReg_Alarm1Date = 0xc,
	RtcReg_Alarm2Date = 0xd,
} RtcRegister;

typedef enum RtcInt1Mode {
	RtcInt1Mode_Disabled  = 0x0,
	RtcInt1Mode_FreqDuty  = 0x1,
	RtcInt1Mode_MinEdge   = 0x2,
	RtcInt1Mode_MinSteady = 0x3,
	RtcInt1Mode_Alarm1    = 0x4,
	RtcInt1Mode_MinSteadyPulse = 0x7,
	RtcInt1Mode_32kHz     = 0x8,
} RtcInt1Mode;

typedef struct RtcDateTime {
	u8 year;    // 2000-2099 (only last two digits)
	u8 month;   // 1-12
	u8 day;     // 1-31
	u8 weekday; // 0-7 starting Monday
	u8 hour;    // 0-23
	u8 minute;  // 0-59
	u8 second;  // 0-59
} RtcDateTime;

MK_CONSTEXPR u8 rtcDecodeBcd(u8 bcd)
{
	return 10*(bcd>>4) + (bcd & 0xf);
}

void rtcInit(void);
void rtcSyncTime(void);

void rtcReadRegister(RtcRegister reg, void* data, size_t size);
void rtcWriteRegister(RtcRegister reg, const void* data, size_t size);

void rtcReadDateTime(RtcDateTime* t);
u32 rtcDateTimeToUnix(const RtcDateTime* t);

MK_INLINE u8 rtcReadRegister8(RtcRegister reg)
{
	u8 value;
	rtcReadRegister(reg, &value, 1);
	return value;
}

MK_INLINE void rtcWriteRegister8(RtcRegister reg, u8 value)
{
	rtcWriteRegister(reg, &value, 1);
}

MK_INLINE u32 rtcReadUnixTime(void)
{
	RtcDateTime t;
	rtcReadDateTime(&t);
	return rtcDateTimeToUnix(&t);
}

MK_EXTERN_C_END
