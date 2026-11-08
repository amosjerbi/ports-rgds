// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#include "../types.h"

// Parameters for svcRegisterRamReset
#define SVC_CLEAR_EWRAM  (1<<0)
#define SVC_CLEAR_IWRAM  (1<<1)
#define SVC_CLEAR_PALRAM (1<<2)
#define SVC_CLEAR_VRAM   (1<<3)
#define SVC_CLEAR_OBJRAM (1<<4)
#define SVC_CLEAR_SERIAL (1<<5)
#define SVC_CLEAR_SOUND  (1<<6)
#define SVC_CLEAR_IO     (1<<7)
#define SVC_CLEAR_ALL    0xff

// Parameters for svcCpuSet/svcCpuFastSet (FastSet only supports 32-bit mode)
#define SVC_SET_SIZE_16(_sz) (((_sz)/2) & 0x1fffff)
#define SVC_SET_SIZE_32(_sz) (((_sz)/4) & 0x1fffff)
#define SVC_SET_FIXED        (1<<24)
#define SVC_SET_UNIT_16      (0<<26)
#define SVC_SET_UNIT_32      (1<<26)

MK_EXTERN_C_START

/* TODO: ABI. This is intended to be returned as r0/r1
typedef struct SvcDivResult {
	s32 quotient;
	s32 remainder;
} SvcDivResult;
*/

typedef struct SvcAffineParams {
	s16 scale_x;
	s16 scale_y;
	u16 angle;
} SvcAffineParams;

typedef struct SvcAffineData {
	s16 pa;
	s16 pb;
	s16 pc;
	s16 pd;
} SvcAffineData;

typedef struct SvcBgAffineParams {
	s32 orig_center_x;
	s32 orig_center_y;
	s16 center_x;
	s16 center_y;
	SvcAffineParams params;
} SvcBgAffineParams;

typedef struct SvcBgAffineData {
	SvcAffineData data;
	s32 start_x;
	s32 start_y;
} SvcBgAffineData;

typedef struct SvcBitUnpackParams {
	u16 in_length_bytes;
	u8  in_width_bits;
	u8  out_width_bits;
	u32 data_offset    : 31;
	u32 zero_data_flag : 1;
} SvcBitUnpackParams;

void svcSoftReset(void) MK_NORETURN;
void svcRegisterRamReset(u32 flags);
void svcHalt(void);
void svcStop(void);
void svcIntrWait(bool waitNext, u32 mask);
void svcVBlankIntrWait(void);
//SvcDivResult svcDiv(s32 num, s32 den);
//SvcDivResult svcDivArm(s32 den, s32 num);
u16 svcSqrt(u32 num);
s16 svcArcTan(s16 value);
u16 svcArcTan2(s32 x, s32 y);
void svcCpuSet(const void* src, void* dst, u32 mode);
void svcCpuFastSet(const void* src, void* dst, u32 mode);
u32 svcGetBiosChecksum(void);
void svcBgAffineSet(const SvcBgAffineParams* in, SvcBgAffineData* out, u32 count);
void svcObjAffineSet(const SvcAffineParams* in, SvcAffineData* out, u32 count, u32 stride);
void svcBitUnpack(const void* src, void* dst, SvcBitUnpackParams const* params);
void svcLZ77UncompWram(const void* src, void* dst);
void svcLZ77UncompVram(const void* src, void* dst);
void svcHuffUncomp(const void* src, void* dst);
void svcRLUncompWram(const void* src, void* dst);
void svcRLUncompVram(const void* src, void* dst);
void svcDiff8bitUnfilterWram(const void* src, void* dst);
void svcDiff8bitUnfilterVram(const void* src, void* dst);
void svcDiff16bitUnfilter(const void* src, void* dst);
void svcSoundBias(bool enable);
u32 svcMidiKey2Freq(const void* wave, u8 key, u8 finetune);
u32 svcMultiBoot(const void* param, u32 mode); // todo: add types
void svcHardReset(void) MK_NORETURN;

MK_EXTERN_C_END
