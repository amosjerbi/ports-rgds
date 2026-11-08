// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#include "../../types.h"

#define MWL_CALIB_NVRAM_OFFSET 0x02a
#define MWL_CALIB_NVRAM_MAX_SZ (0x200-MWL_CALIB_NVRAM_OFFSET)

MK_EXTERN_C_START

typedef struct MwlCalibData {
	u16 crc16;
	u16 total_len;
	u8  unk_0x02e;
	u8  version;
	u8  unk_0x030[6];
	u16 mac_addr[3];
	u16 enabled_ch_mask;
	u16 op_flags;
	//---- 0x040 ----
	u8  rf_type; // 2=DS; 3=DS Lite and up
	u8  rf_entry_bits; // 8 or 24
	u8  rf_num_entries;
	u8  rf_num_regs;
	u16 mac_reg_init[0x10];
	u8  bb_reg_init[0x69];
	u8  pad_0x0cd;
	//---- 0x0ce ----
	u8  rf_entries[]; // rf_num_entries*rf_entry_bits/8

	// Afterwards:
	//   union {
	//     MwlChanCalibV2 v2; // at 0x0f2 (after 12 24-bit entries)
	//     MwlChanCalibV3 v3; // at 0x0f7 (after 41  8-bit entries)
	//   };
} MwlCalibData;

typedef struct MwlChanRegV2 {
	u8 reg_0x05[3];
	u8 reg_0x06[3];
} MwlChanRegV2;

typedef struct MwlChanCalibV2 {
	MwlChanRegV2 rf_regs[14];
	u8 bb_0x1e_values[14];
	u8 rf_0x09_bits[14];
} MwlChanCalibV2;

typedef struct MwlChanRegV3 {
	u8 index;
	u8 values[14];
} MwlChanRegV3;

typedef struct MwlChanCalibV3 {
	u8 bb_num_regs;
	MwlChanRegV3 regs[];
	// Actually:
	//   MwlChanRegV3 bb_regs[bb_num_regs];
	//   MwlChanRegV3 rf_regs[rf_num_regs];
} MwlChanCalibV3;

bool mwlCalibLoad(void);

alignas(2) extern u8 g_mwlCalibData[MWL_CALIB_NVRAM_MAX_SZ];

MK_INLINE MwlCalibData* mwlGetCalibData(void)
{
	return (MwlCalibData*)g_mwlCalibData;
}

MK_EXTERN_C_END
