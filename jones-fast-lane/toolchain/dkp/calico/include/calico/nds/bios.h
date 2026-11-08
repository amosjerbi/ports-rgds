// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#include "../types.h"

/*! @addtogroup bios
	@{
*/

// Parameters for svcCpuSet
#define SVC_SET_SIZE_16(_sz) (((_sz)/2) & 0x1fffff)
#define SVC_SET_SIZE_32(_sz) (((_sz)/4) & 0x1fffff)
#define SVC_SET_FIXED        (1<<24)
#define SVC_SET_UNIT_16      (0<<26)
#define SVC_SET_UNIT_32      (1<<26)

#define SVC_RSA_HEAP_SZ      0x1000
#define SVC_RSA_BUFFER_SZ    0x80
#define SVC_SHA1_DIGEST_SZ   0x14

MK_EXTERN_C_START

typedef struct SvcDivResult {
	s32 quot;
	s32 rem;
} SvcDivResult;

typedef struct SvcBitUnpackParams {
	u16 in_length_bytes;
	u8  in_width_bits;
	u8  out_width_bits;
	u32 data_offset    : 31;
	u32 zero_data_flag : 1;
} SvcBitUnpackParams;

typedef struct SvcRsaHeapContext {
	void*  start;
	void*  end;
	size_t size;
} SvcRsaHeapContext;

typedef struct SvcRsaParams {
	void* output;
	const void* input;
	const void* key;
} SvcRsaParams;

typedef struct SvcSha1Context {
	u32 state[5];
	u32 total[2];
	u8  frag_buffer[64];
	u32 frag_size;
	void (*hash_block)(struct SvcSha1Context* ctx, const void* data, size_t size); // MUST be set to 0 before init!
} SvcSha1Context;

void svcWaitByLoop(s32 loop_cycles);
void svcHalt(void);
#ifdef ARM7
void svcSleep(void);
void svcSoundBias(bool enable, u32 delay_count);
#endif
u64 svcDivModImpl(s32 num, s32 den) __asm__("svcDivMod"); //!< @private
void svcCpuSet(const void* src, void* dst, u32 mode);
u16 svcSqrt(u32 num);
u16 svcGetCRC16(u16 initial_crc, const void* mem, u32 size);
void svcBitUnpack(const void* src, void* dst, SvcBitUnpackParams const* params);
void svcLZ77UncompWram(const void* src, void* dst);
void svcRLUncompWram(const void* src, void* dst);
void svcDiff8bitUnfilterWram(const void* src, void* dst);
void svcDiff16bitUnfilter(const void* src, void* dst);

MK_INLINE SvcDivResult svcDivMod(s32 num, s32 den)
{
	union {
		u64 ret;
		SvcDivResult res;
	} u = { svcDivModImpl(num, den) };
	return u.res;
}

#ifdef ARM7
MK_NORETURN void svcCustomHalt(unsigned mode);
#endif

void svcRsaHeapInitTWL(SvcRsaHeapContext* ctx, void* mem, size_t size);
bool svcRsaDecryptRawTWL(SvcRsaHeapContext* ctx, const SvcRsaParams* params, size_t* out_size);
bool svcRsaDecryptUnpadTWL(SvcRsaHeapContext* ctx, void* output, const void* input, const void* key); // sizeof(output) >= SVC_RSA_BUFFER_SZ
bool svcRsaDecryptDerSha1TWL(SvcRsaHeapContext* ctx, void* output, const void* input, const void* key);

void svcSha1InitTWL(SvcSha1Context* ctx);
void svcSha1UpdateTWL(SvcSha1Context* ctx, const void* data, size_t size);
void svcSha1DigestTWL(void* digest, SvcSha1Context* ctx);
void svcSha1CalcTWL(void* digest, const void* data, size_t size);
bool svcSha1VerifyTWL(const void* lhs, const void* rhs);
void svcSha1RandomTWL(void* output, size_t out_size, const void* seed, size_t seed_size);

MK_EXTERN_C_END

//! @}
