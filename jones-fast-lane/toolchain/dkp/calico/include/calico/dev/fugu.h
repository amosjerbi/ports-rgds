// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#include "../types.h"
#include "../arm/common.h"

// Nintendo-style Blowfish cipher implementation (hence "Fugu")

#define FUGU_NUM_ROUNDS 16

MK_EXTERN_C_START

typedef struct FuguState {
	u32 P[FUGU_NUM_ROUNDS+2];
	u32 S[4][256];
} FuguState;

typedef struct FuguNtr {
	FuguState state;
	u32 key[3];
} FuguNtr;

MK_EXTERN32 void fuguKeySchedule(FuguState* state, const u32* key, unsigned key_num_words);
MK_EXTERN32 void fuguEncrypt(FuguState const* state, u32 buf[2]);
MK_EXTERN32 void fuguDecrypt(FuguState const* state, u32 buf[2]);

MK_INLINE void fuguNtrInit(FuguNtr* ctx, const void* initial_state, u32 key)
{
	armCopyMem32(&ctx->state, initial_state, sizeof(ctx->state));
	ctx->key[0] = key;
	ctx->key[1] = key>>1;
	ctx->key[2] = key<<1;
}

MK_INLINE void fuguNtrKeySchedule(FuguNtr* ctx, bool full_key, bool do_tweak)
{
	if (do_tweak) {
		ctx->key[1] <<= 1;
		ctx->key[2] >>= 1;
	}
	fuguEncrypt(&ctx->state, &ctx->key[1]);
	fuguEncrypt(&ctx->state, &ctx->key[0]);
	fuguKeySchedule(&ctx->state, ctx->key, full_key ? 3 : 2);
}

MK_EXTERN_C_END
