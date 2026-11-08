// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#include "../../types.h"
#include "../wlan.h"

MK_EXTERN_C_START

typedef enum MwlMode {
	MwlMode_Test       = 0,
	MwlMode_LocalHost  = 1,
	MwlMode_LocalGuest = 2,
	MwlMode_Infra      = 3,
} MwlMode;

typedef enum MwlStatus {
	MwlStatus_Idle   = 0,
	MwlStatus_Class1 = 1, // 802.11 Class 1 = Unauthenticated & Unassociated
	MwlStatus_Class2 = 2, // 802.11 Class 2 = Authenticated   & Unassociated
	MwlStatus_Class3 = 3, // 802.11 Class 3 = Authenticated   & Associated
} MwlStatus;

typedef enum MwlRxType {
	MwlRxType_IeeeMgmtOther = 0,
	MwlRxType_IeeeBeacon    = 1,
	MwlRxType_IeeeCtrl      = 5,
	MwlRxType_IeeeData      = 8,
	MwlRxType_MpCmdFrame    = 12,
	MwlRxType_MpEndFrame    = 13,
	MwlRxType_MpReplyFrame  = 14,
	MwlRxType_Null          = 15,
} MwlRxType;

typedef struct MwlDataTxHdr {
	u16 status;
	u16 mp_aid_mask;
	u16 retry_count;
	u16 app_rate;
	u16 service_rate;
	u16 mpdu_len;
} MwlDataTxHdr;

typedef struct MwlDataRxHdr {
	u16 status;
	u16 next_frame_offset;
	u16 timestamp;
	u16 service_rate;
	u16 mpdu_len;
	u16 rssi;
} MwlDataRxHdr;

MK_CONSTEXPR u8 mwlDecodeRssi(u16 in)
{
	unsigned ret = (in & 0xfc) >> 2;
	if (!(in & 2)) ret += 25;
	return ret;
}

MK_EXTERN_C_END
