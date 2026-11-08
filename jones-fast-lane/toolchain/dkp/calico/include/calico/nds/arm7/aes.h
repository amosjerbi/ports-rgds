// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#if !defined(__NDS__) || !defined(ARM7)
#error "This header file is only for NDS ARM7"
#endif

#include "../../types.h"
#include "../io.h"

#define AES_BLOCK_SZ       0x10
#define AES_BLOCK_SZ_WORDS (AES_BLOCK_SZ/sizeof(u32))

#define AES_FIFO_SZ_WORDS  0x10
#define AES_FIFO_SZ        (AES_FIFO_SZ_WORDS*sizeof(u32))

#define AES_MAX_PAYLOAD_SZ 0xffff0

#define REG_AES_CNT          MK_REG(u32, IO_AES_CNT)
#define REG_AES_LEN          MK_REG(u32, IO_AES_LEN)
#define REG_AES_WRFIFO       MK_REG(u32, IO_AES_WRFIFO)
#define REG_AES_RDFIFO       MK_REG(u32, IO_AES_RDFIFO)
#define REG_AES_IV           MK_REG(AesBlock, IO_AES_IV)
#define REG_AES_MAC          MK_REG(AesBlock, IO_AES_MAC)
#define REG_AES_SLOTxKEY(_x) MK_REG(AesBlock, IO_AES_SLOTxKEY(_x))
#define REG_AES_SLOTxX(_x)   MK_REG(AesBlock, IO_AES_SLOTxX(_x))
#define REG_AES_SLOTxY(_x)   MK_REG(AesBlock, IO_AES_SLOTxY(_x))

#define AES_WRFIFO_COUNT(_x)    ((_x) & 0x1f)
#define AES_RDFIFO_COUNT(_x)    (((_x)>>5) & 0x1f)
#define AES_CCM_VERIFY_VALID    (1U<<21)
#define AES_KEY_SCHEDULE_BUSY   (1U<<25)
#define AES_BUSY                (1U<<31)

#define AES_WRFIFO_FLUSH        (1U<<10)
#define AES_RDFIFO_FLUSH        (1U<<11)
#define AES_WRFIFO_DMA_SIZE(_x) (((_x)&3)<<12)
#define AES_RDFIFO_DMA_SIZE(_x) (((_x)&3)<<14)
#define AES_CCM_MAC_SIZE(_x)    ((((_x)/2-1)&7)<<16)
#define AES_CCM_PASSTHROUGH     (1U<<19)
#define AES_CCM_VERIFY_WRFIFO   (0U<<20)
#define AES_CCM_VERIFY_MAC      (1U<<20)
#define AES_KEY_SELECT          (1U<<24)
#define AES_KEY_SLOT(_x)        (((_x)&3)<<26)
#define AES_MODE(_x)            (((_x)&3)<<28)
#define AES_IRQ_ENABLE          (1U<<30)
#define AES_ENABLE              (1U<<31)

MK_EXTERN_C_START

typedef struct AesBlock {
	u32 data[AES_BLOCK_SZ_WORDS];
} AesBlock;

typedef enum AesWrfifoDmaSize {
	AesWrfifoDma_16 = 0,
	AesWrfifoDma_12 = 1,
	AesWrfifoDma_8  = 2,
	AesWrfifoDma_4  = 3,
} AesWrfifoDmaSize;

typedef enum AesRdfifoDmaSize {
	AesRdfifoDma_4  = 0,
	AesRdfifoDma_8  = 1,
	AesRdfifoDma_12 = 2,
	AesRdfifoDma_16 = 3,
} AesRdfifoDmaSize;

typedef enum AesKeySlot {
	AesKeySlot_Unk0   = 0,
	AesKeySlot_System = 1,
	AesKeySlot_Unk1   = 2,
	AesKeySlot_Nand   = 3,
} AesKeySlot;

typedef enum AesMode {
	AesMode_CcmDecrypt = 0,
	AesMode_CcmEncrypt = 1,
	AesMode_Ctr        = 2,
} AesMode;

#if !__thumb__

MK_INLINE void aesCtrIncrementIv(AesBlock* iv, u32 value)
{
	__asm__ __volatile__ (
		"adds   %0, %0, %4\n\t"
		"addcss %1, %1, #1\n\t"
		"addcss %2, %2, #1\n\t"
		"addcs  %3, %3, #1\n\t"
		: "+r"(iv->data[0]), "+r"(iv->data[1]), "+r"(iv->data[2]), "+r"(iv->data[3])
		: "Ir"(value)
		: "cc"
	);
}

#else

MK_INLINE void aesCtrIncrementIv(AesBlock* iv, u32 value)
{
	__asm__ __volatile__ (
		"add %0, %4\n\t"
		"adc %1, %5\n\t"
		"adc %2, %5\n\t"
		"adc %3, %5\n\t"
		: "+l"(iv->data[0]), "+l"(iv->data[1]), "+l"(iv->data[2]), "+l"(iv->data[3])
		: "Il"(value), "l"(0)
		: "cc"
	);
}

#endif

MK_INLINE void aesBusyWaitReady(void)
{
	while (REG_AES_CNT & (AES_BUSY | AES_KEY_SCHEDULE_BUSY));
}

MK_INLINE void aesBusyWaitWrFifoReady(void)
{
	while (AES_WRFIFO_COUNT(REG_AES_CNT) > 0);
}

MK_INLINE void aesBusyWaitRdFifoReady(void)
{
	while (AES_RDFIFO_COUNT(REG_AES_CNT) < 0x10);
}

MK_INLINE void aesSelectKeySlot(AesKeySlot slot)
{
	REG_AES_CNT = (REG_AES_CNT &~ AES_KEY_SLOT(3)) | AES_KEY_SLOT(slot) | AES_KEY_SELECT;
	while (REG_AES_CNT & AES_KEY_SCHEDULE_BUSY);
}

MK_EXTERN_C_END
