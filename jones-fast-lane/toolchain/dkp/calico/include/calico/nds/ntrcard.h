// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#if !defined(__NDS__)
#error "This header file is only for NDS"
#endif

#include "../types.h"
#include "io.h"

/*! @addtogroup ntrcard

	See https://problemkaputt.de/gbatek-ds-cartridge-protocol.htm for more details
	on the DS gamecard protocol.

	@{
*/

//! Size in bytes of a DS gamecard sector
#define NTRCARD_SECTOR_SZ 0x200

/*! @name DS gamecard memory mapped I/O
	@{
*/

#define REG_NTRCARD_CNT       MK_REG(u16, IO_NTRCARD_CNT)
#define REG_NTRCARD_SPIDATA   MK_REG(u16, IO_NTRCARD_SPIDATA)

#define REG_NTRCARD_ROMCNT    MK_REG(u32, IO_NTRCARD_ROMCNT)
#define REG_NTRCARD_ROMCMD    MK_REG(u64, IO_NTRCARD_ROMCMD) // big endian!
#define REG_NTRCARD_ROMCMD_HI MK_REG(u32, IO_NTRCARD_ROMCMD+0)
#define REG_NTRCARD_ROMCMD_LO MK_REG(u32, IO_NTRCARD_ROMCMD+4)
#define REG_NTRCARD_SEED0_LO  MK_REG(u32, IO_NTRCARD_SEED0_LO)
#define REG_NTRCARD_SEED1_LO  MK_REG(u32, IO_NTRCARD_SEED1_LO)
#define REG_NTRCARD_SEED0_HI  MK_REG(u16, IO_NTRCARD_SEED0_HI)
#define REG_NTRCARD_SEED1_HI  MK_REG(u16, IO_NTRCARD_SEED1_HI)
#define REG_NTRCARD_FIFO      MK_REG(u32, IO_NTRCARD_FIFO)

#define NTRCARD_CNT_SPI_BAUD(_n) ((_n)&3)
#define NTRCARD_CNT_SPI_HOLD     (1U<<6)
#define NTRCARD_CNT_SPI_BUSY     (1U<<7)
#define NTRCARD_CNT_MODE_ROM     (0U<<13)
#define NTRCARD_CNT_MODE_SPI     (1U<<13)
#define NTRCARD_CNT_TX_IE        (1U<<14)
#define NTRCARD_CNT_ENABLE       (1U<<15)

#define NTRCARD_ROMCNT_GAP1_LEN(_n) ((_n)&0x1fff)
#define NTRCARD_ROMCNT_ENCR_DATA    (1U<<13)
#define NTRCARD_ROMCNT_ENCR_ENABLE  (1U<<14)
#define NTRCARD_ROMCNT_SEED_APPLY   (1U<<15)
#define NTRCARD_ROMCNT_GAP2_LEN(_n) (((_n)&0x3f)<<16)
#define NTRCARD_ROMCNT_ENCR_CMD     (1U<<22)
#define NTRCARD_ROMCNT_DATA_READY   (1U<<23)
#define NTRCARD_ROMCNT_BLK_SIZE(n)  (((n)&7)<<24)
#define NTRCARD_ROMCNT_CLK_DIV_5    (0U<<27)
#define NTRCARD_ROMCNT_CLK_DIV_8    (1U<<27)
#define NTRCARD_ROMCNT_CLK_GAP_ON   (1U<<28)
#define NTRCARD_ROMCNT_NO_RESET     (1U<<29)
#define NTRCARD_ROMCNT_WRITE        (1U<<30)
#define NTRCARD_ROMCNT_START        (1U<<31)
#define NTRCARD_ROMCNT_BUSY         NTRCARD_ROMCNT_START

//! @}

MK_EXTERN_C_START

//! Baudrates for the SPI interface (save backup) in DS gamecards
typedef enum NtrCardSpiBaud {
	NtrCardSpiBaud_4MHz = 0,
	NtrCardSpiBaud_2MHz,
	NtrCardSpiBaud_1MHz,
	NtrCardSpiBaud_512KHz,
} NtrCardSpiBaud;

//! DS gamecard block transfer sizes
typedef enum NtrCardBlkSize {
	NtrCardBlkSize_0      = 0,
	NtrCardBlkSize_0x200  = 1,
	NtrCardBlkSize_0x400  = 2,
	NtrCardBlkSize_0x800  = 3,
	NtrCardBlkSize_0x1000 = 4,
	NtrCardBlkSize_0x2000 = 5,
	NtrCardBlkSize_0x4000 = 6,
	NtrCardBlkSize_4      = 7,

	NtrCardBlkSize_Sector = NtrCardBlkSize_0x200,
} NtrCardBlkSize;

//! DS gamecard operation modes
typedef enum NtrCardMode {
	NtrCardMode_None   = 0, //!< Uninitialized/undefined
	NtrCardMode_Init   = 1, //!< Initialization/Unencrypted Load mode (unencrypted commands and data)
	NtrCardMode_Secure = 2, //!< Secure Area Load mode (Blowfish-encrypted commands, unencrypted or PNG-encrypted data)
	NtrCardMode_Main   = 3, //!< Main Data Load mode (PNG-encrypted commands and data)
} NtrCardMode;

//! DS gamecard commands
typedef enum NtrCardCmd {
	// Initialization mode commands (unencrypted)
	NtrCardCmd_Init               = 0x9f,
	NtrCardCmd_InitGetChipId      = 0x90,
	NtrCardCmd_InitRomRead        = 0x00,
	NtrCardCmd_InitEnterSecureNtr = 0x3c,
	NtrCardCmd_InitEnterSecureTwl = 0x3d,

	// Secure mode commands (encrypted with Blowfish aka "KEY1")
	NtrCardCmd_SecurePngOn        = 0x4,
	NtrCardCmd_SecurePngOff       = 0x6,
	NtrCardCmd_SecureGetChipId    = 0x1,
	NtrCardCmd_SecureReadBlock    = 0x2,
	NtrCardCmd_SecureEnterMain    = 0xa,

	// Main mode commands (encrypted with PNG aka "KEY2")
	NtrCardCmd_MainGetChipId      = 0xb8,
	NtrCardCmd_MainRomRead        = 0xb7,
	NtrCardCmd_MainGetStatus      = 0xd6,
	NtrCardCmd_MainRomRefresh     = 0xb5,
} NtrCardCmd;

//! DS gamecard chip ID
typedef union NtrChipId {
	u32 raw;
	struct {
		u32 manuf       : 8;

		u32 chip_size   : 8;

		u32 has_ir      : 1;
		u32 unk17       : 1;
		u32 _pad18      : 5;
		u32 unk23       : 1;

		u32 _pad24      : 3;
		u32 is_nand     : 1;
		u32 is_ctr      : 1;
		u32 has_refresh : 1;
		u32 is_twl      : 1;
		u32 is_1trom    : 1;
	};
} NtrChipId;

//! @private
typedef struct NtrCardSecure {
	u32 romcnt;
	u32 fixed_arg;
	u32 counter;
	u16 delay;
	u16 is_1trom : 1;
	u16 is_twl   : 1;
	u16          : 14;
} NtrCardSecure;

//! @private
typedef union NtrCardSecureCmd {
	u32 raw[2];
	struct {
		u64 counter : 20;
		u64 fixed   : 24;
		u64 param   : 16;
		u64 cmd     : 4;
	};
} NtrCardSecureCmd;

//! Calculates ROM size of a DS gamecard using its chip @p id
MK_CONSTEXPR u32 ntrcardCalcChipSize(NtrChipId id)
{
	unsigned val = id.chip_size;
	if (val < 0xf0) {
		return 0x100000 * (val+1);
	} else {
		return 0x10000000 * (0x100-val);
	}
}

/*! @brief Brokers ownership and access to the DS gamecard from the current CPU
	@return true on success, false if the hardware is in use by the other CPU

	During the first call to this function, Calico tries to guess the current
	state of the DS gamecard. If the application was booted from Slot-1 (see
	@ref EnvBootSrc_Card), the mode is assumed to be @ref NtrCardMode_Main,
	otherwise the mode is left undefined (@ref NtrCardMode_None).

	When DLDI drivers for Slot-1 flashcards are loaded, the ARM7 takes ownership
	of the DS gamecard; which means calling this function from the ARM9 will fail.
*/
bool ntrcardOpen(void);

//! Releases DS gamecard ownership obtained by @ref ntrcardOpen
void ntrcardClose(void);

//! Returns the current DS gamecard operation mode (see @ref NtrCardMode)
NtrCardMode ntrcardGetMode(void);

//! Clears the DS gamecard state, returning to @ref NtrCardMode_None
void ntrcardClearState(void);

//! Sets the DS gamecard @p params (NTRCARD_ROMCNT_GAP1/2_LEN, NTRCARD_ROMCNT_CLK_DIV_5/8)
void ntrcardSetParams(u32 params);

/*! @brief Initializes the DS gamecard
	@param[in] dma_ch @ref dma channel to use, or a negative number to use CPU-driven transfers
	@return true on success, false on failure
	@note The current mode must be @ref NtrCardMode_None. Upon success, the mode becomes @ref NtrCardMode_Init.
*/
bool ntrcardStartup(int dma_ch);

/*! @brief Reads the chip ID from the DS gamecard
	@param[out] out @ref NtrChipId output
	@return true on success, false on failure
	@note The current mode must be @ref NtrCardMode_Init or @ref NtrCardMode_Main
*/
bool ntrcardGetChipId(NtrChipId* out);

/*! @brief Reads a single sector from the DS gamecard ROM
	@param[in] dma_ch @ref dma channel to use, or a negative number to use CPU-driven transfers
	@param[in] offset Sector address within the ROM
	@param[out] buf Output buffer (must hold @ref NTRCARD_SECTOR_SZ bytes)
	@return true on success, false on failure
	@note The current mode must be @ref NtrCardMode_Init or @ref NtrCardMode_Main. DS gamecards
	typically restrict which data is readable depending on the mode, see gbatek for more details.
	@warning When using DMA on the ARM9 the output buffer must be outside ITCM/DTCM, and cache-aligned.
	Otherwise the output buffer must be word-aligned.
*/
bool ntrcardRomReadSector(int dma_ch, u32 offset, void* buf);

/*! @brief Reads data from the DS gamecard ROM
	@param[in] dma_ch @ref dma channel to use, or a negative number to use CPU-driven transfers
	@param[in] offset Byte address within the ROM
	@param[out] buf Output buffer
	@param[in] size Size in bytes of the buffer
	@return true on success, false on failure
	@note The current mode must be @ref NtrCardMode_Init or @ref NtrCardMode_Main. DS gamecards
	typically restrict which data is readable depending on the mode, see gbatek for more details.
	@note Unlike @ref ntrcardRomReadSector, this function is a higher level wrapper that allows
	reading an arbitrary sized buffer at an arbitrary byte address (including partial and/or multiple sector reads).
	Moreover, this function has no buffer location or alignment restrictions; although it is faster
	to follow the same guidelines as @ref ntrcardRomReadSector.
*/
bool ntrcardRomRead(int dma_ch, u32 offset, void* buf, u32 size);

//! @private
bool ntrcardEnterSecure(NtrCardSecure* secure);
//! @private
bool ntrcardSecureCmd(int dma_ch, NtrCardSecureCmd const* cmd, NtrCardBlkSize sz, void* buf);
//! @private
bool ntrcardSetPngSeed(u64 seed0, u64 seed1);
//! @private
bool ntrcardLeaveSecure(void);

MK_EXTERN_C_END

//! @}
