// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#if !defined(__NDS__) || !defined(ARM7)
#error "This header file is only for NDS ARM7"
#endif

#include "../../types.h"
#include "../io.h"
#include "../mic.h"
#include "tsc.h"
#include "codec.h"

#define REG_MICEX_CNT  MK_REG(u16, IO_MICEX_CNT)
#define REG_MICEX_DATA MK_REG(u32, IO_MICEX_DATA)

#define MICEX_CNT_NO_L         (1U<<0)
#define MICEX_CNT_NO_R         (1U<<1)
#define MICEX_CNT_RATE_DIV(_x) (((_x)&3)<<2)
#define MICEX_CNT_FIFO_EMPTY   (1U<<8)
#define MICEX_CNT_FIFO_HALF    (1U<<9)
#define MICEX_CNT_FIFO_FULL    (1U<<10)
#define MICEX_CNT_FIFO_BORKED  (1U<<11)
#define MICEX_CNT_CLEAR_FIFO   (1U<<12)
#define MICEX_CNT_IE_FIFO_HALF (1U<<13)
#define MICEX_CNT_IE_FIFO_FULL (1U<<14)
#define MICEX_CNT_ENABLE       (1U<<15)

MK_EXTERN_C_START

void micStartServer(u8 thread_prio);

MK_EXTERN_C_END
