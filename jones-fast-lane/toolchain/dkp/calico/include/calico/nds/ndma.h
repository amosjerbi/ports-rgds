// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#include "../types.h"
#include "../system/sysclock.h"

#if defined(__NDS__)
#include "../nds/io.h"
#else
#error "This header file is only for DSi"
#endif

/*! @addtogroup ndma
	@{
*/
/*! @page ndma_size Explanation of the three sizes that affect a NDMA operation
	1. Block size (`REG_NDMAxCNT.NDMA_BLK_WORDS(x)`):
		Size of the atomic transfer unit in words. Usually corresponds to
		FIFO size (e.g. AES has 0x10 words, SDMMC has 512 bytes = 0x80 words)
	2. Single transfer size (`REG_NDMAxWCNT`):
		Size of a single transfer in words. Must be a multiple of block size.
	3. Total operation size (`REG_NDMAxTCNT`):
		Total desired size of the operation in words. Only used by NdmaTxMode_Timing.
		The operation is split up into multiple transfers. The last one
		may be smaller in order to copy the remaining data.
*/

/*! @name NDMA registers
	@{
*/

#define REG_NDMAGCNT       MK_REG(u32, IO_NDMAGCNT)
#define REG_NDMAxSAD(_x)   MK_REG(u32, IO_NDMAxSAD(_x))
#define REG_NDMAxDAD(_x)   MK_REG(u32, IO_NDMAxDAD(_x))
#define REG_NDMAxTCNT(_x)  MK_REG(u32, IO_NDMAxTCNT(_x))
#define REG_NDMAxWCNT(_x)  MK_REG(u32, IO_NDMAxWCNT(_x))
#define REG_NDMAxBCNT(_x)  MK_REG(u32, IO_NDMAxBCNT(_x))
#define REG_NDMAxFDATA(_x) MK_REG(u32, IO_NDMAxFDATA(_x))
#define REG_NDMAxCNT(_x)   MK_REG(u32, IO_NDMAxCNT(_x))

#define NDMA_G_RR_CYCLES(_x) ((__builtin_ffs(_x)&0xf)<<16)
#define NDMA_G_FIXED_PRIO    (0<<31)
#define NDMA_G_ROUND_ROBIN   (1<<31)

#define NDMA_B_PRESCALER_1  (0<<16)
#define NDMA_B_PRESCALER_4  (1<<16)
#define NDMA_B_PRESCALER_16 (2<<16)
#define NDMA_B_PRESCALER_64 (3<<16)

#define NDMA_DST_MODE(_x)  (((_x)&3)<<10)
#define NDMA_DST_RELOAD    (1<<12)
#define NDMA_SRC_MODE(_x)  (((_x)&3)<<13)
#define NDMA_SRC_RELOAD    (1<<15)
#define NDMA_BLK_WORDS(_x) (((__builtin_ffs(_x)-1)&0xf)<<16)
#define NDMA_TIMING(_x)    (((_x)&0xf)<<24)
#define NDMA_TX_MODE(_x)   (((_x)&3)<<28)
#define NDMA_IRQ_ENABLE    (1<<30)
#define NDMA_START         (1<<31)

//! @}

MK_EXTERN_C_START

//! NDMA address mode
typedef enum NdmaMode {
	NdmaMode_Increment = 0, //!< Increments the address after every word
	NdmaMode_Decrement = 1, //!< Like NdmaMode_Increment, but decrements instead
	NdmaMode_Fixed     = 2, //!< Keeps the address fixed after every word
	NdmaMode_FillData  = 3, //!< Source only: Uses the value of REG_NDMAxFDATA instead as source data
} NdmaMode;

//! NDMA transfer mode
typedef enum NdmaTxMode {
	NdmaTxMode_Timing       = 0, //!< Transfer is synchronized according to @ref NdmaTiming. NDMA performs multiple transfers (of size REG_NDMAxWCNT) until reaching the total size (REG_NDMAxTCNT). The last transfer may be smaller
	NdmaTxMode_Immediate    = 1, //!< Transfer starts immediately. NDMA only performs a single transfer (of size REG_NDMAxWCNT)
	NdmaTxMode_TimingRepeat = 2, //!< Like NdmaTxMode_Timing, but with an indefinite number of transfers (of size REG_NDMAxWCNT)
} NdmaTxMode;

//! NDMA transfer timing mode
typedef enum NdmaTiming {
	NdmaTiming_Timer0    = 0,  //!< Transfer is driven by @ref timer channel 0
	NdmaTiming_Timer1    = 1,  //!< Transfer is driven by @ref timer channel 1
	NdmaTiming_Timer2    = 2,  //!< Transfer is driven by @ref timer channel 2
	NdmaTiming_Timer3    = 3,  //!< Transfer is driven by @ref timer channel 3
	NdmaTiming_Slot1     = 4,  //!< Transfer is driven by DS gamecard
	NdmaTiming_Slot1_2   = 5,  //!< Like NdmaTiming_Slot1, but for the second DS gamecard slot (DSi prototype)
	NdmaTiming_VBlank    = 6,  //!< Transfer starts at VBlank
#if defined(ARM7)
	NdmaTiming_Mitsumi   = 7,  //!< Transfer is driven by Mitsumi wireless
	NdmaTiming_Tmio0     = 8,  //!< Transfer is driven by TMIO0 FIFO (connected to SD/eMMC)
	NdmaTiming_Tmio1     = 9,  //!< Transfer is driven by TMIO1 FIFO (connected to Atheros wireless)
	NdmaTiming_AesWrFifo = 10, //!< Transfer is driven by the AES write FIFO
	NdmaTiming_AesRdFifo = 11, //!< Transfer is driven by the AES read FIFO
	NdmaTiming_MicData   = 12, //!< Transfer is driven by the microphone FIFO
#elif defined(ARM9)
	NdmaTiming_HBlank    = 7,  //!< Transfer starts at HBlank
	NdmaTiming_DispStart = 8,  //!< Transfer is synchronized with the start of the display
	NdmaTiming_MemDisp   = 9,  //!< Transfer is used to support main memory display
	NdmaTiming_3dFifo    = 10, //!< Transfer is driven by 3D geometry command FIFO
	NdmaTiming_Camera    = 11, //!< Transfer is driven by the cameras
#endif
} NdmaTiming;

//! Calculates the value of REG_NDMAxTCNT for a given @p prescaler (NDMA_B_PRESCALER_\*) and @p freq (in Hz)
MK_CONSTEXPR u32 ndmaCalcBlockTimer(unsigned prescaler, unsigned freq)
{
	unsigned basefreq = SYSTEM_CLOCK;
	basefreq >>= ((prescaler>>16)&2) * 2;
	unsigned interval = (basefreq + freq/2) / freq;
	if (interval < 1) interval = 1;
	else if (interval > 0xffff) interval = 0xffff;
	return interval | prescaler;
}

//! Returns true if NDMA channel @p id is active
MK_INLINE bool ndmaIsBusy(unsigned id)
{
	return REG_NDMAxCNT(id) & NDMA_START;
}

//! Waits for NDMA channel @p id to be idle, using a busy loop
MK_INLINE void ndmaBusyWait(unsigned id)
{
	while (ndmaIsBusy(id));
}

/*! @brief Starts an immediate NDMA transfer on channel @p id
	@param[out] dst Destination address
	@param[in] src Source address
	@param[in] size Length of the transfer in bytes
*/
MK_INLINE void ndmaStartCopy32(unsigned id, void* dst, const void* src, size_t size)
{
	REG_NDMAxSAD(id) = (u32)src;
	REG_NDMAxDAD(id) = (u32)dst;
	REG_NDMAxBCNT(id) = 0;
	REG_NDMAxWCNT(id) = size/4;
	REG_NDMAxCNT(id) =
		NDMA_DST_MODE(NdmaMode_Increment) |
		NDMA_SRC_MODE(NdmaMode_Increment) |
		NDMA_BLK_WORDS(1) |
		NDMA_TX_MODE(NdmaTxMode_Immediate) |
		NDMA_START;
}

/*! @brief Starts an immediate NDMA fill on channel @p id
	@param[out] dst Destination address
	@param[in] value 32-bit fill value
	@param[in] size Length of the fill in bytes
*/
MK_INLINE void ndmaStartFill32(unsigned id, void* dst, u32 value, size_t size)
{
	REG_NDMAxFDATA(id) = value;
	REG_NDMAxDAD(id) = (u32)dst;
	REG_NDMAxBCNT(id) = 0;
	REG_NDMAxWCNT(id) = size/4;
	REG_NDMAxCNT(id) =
		NDMA_DST_MODE(NdmaMode_Increment) |
		NDMA_SRC_MODE(NdmaMode_FillData) |
		NDMA_BLK_WORDS(1) |
		NDMA_TX_MODE(NdmaTxMode_Immediate) |
		NDMA_START;
}

MK_EXTERN_C_END

//! @}
