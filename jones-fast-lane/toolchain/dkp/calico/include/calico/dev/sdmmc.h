// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#include "../types.h"
#include "tmio.h"

#define SDMMC_CMD_GO_IDLE               (TMIO_CMD_INDEX(0)  | TMIO_CMD_RESP_NONE)
#define SDMMC_CMD_SEND_OP_COND          (TMIO_CMD_INDEX(1)  | TMIO_CMD_RESP_48_NOCRC)
#define SDMMC_CMD_ALL_GET_CID           (TMIO_CMD_INDEX(2)  | TMIO_CMD_RESP_136)
#define SDMMC_CMD_MMC_SET_RELATIVE_ADDR (TMIO_CMD_INDEX(3)  | TMIO_CMD_RESP_48)
#define SDMMC_CMD_SD_GET_RELATIVE_ADDR  (TMIO_CMD_INDEX(3)  | TMIO_CMD_RESP_48)
#define SDMMC_CMD_MMC_SWITCH            (TMIO_CMD_INDEX(6)  | TMIO_CMD_RESP_48_BUSY)
#define SDMMC_CMD_SELECT_CARD           (TMIO_CMD_INDEX(7)  | TMIO_CMD_RESP_48_BUSY)
#define SDMMC_CMD_SET_IF_COND           (TMIO_CMD_INDEX(8)  | TMIO_CMD_RESP_48)
#define SDMMC_CMD_GET_CSD               (TMIO_CMD_INDEX(9)  | TMIO_CMD_RESP_136)
#define SDMMC_CMD_GET_STATUS            (TMIO_CMD_INDEX(13) | TMIO_CMD_RESP_48)
#define SDMMC_CMD_SET_BLOCKLEN          (TMIO_CMD_INDEX(16) | TMIO_CMD_RESP_48)
#define SDMMC_CMD_READ_MULTIPLE_BLOCK   (TMIO_CMD_INDEX(18) | TMIO_CMD_RESP_48 | TMIO_CMD_TX | TMIO_CMD_TX_READ  | TMIO_CMD_TX_MULTI)
#define SDMMC_CMD_WRITE_MULTIPLE_BLOCK  (TMIO_CMD_INDEX(25) | TMIO_CMD_RESP_48 | TMIO_CMD_TX | TMIO_CMD_TX_WRITE | TMIO_CMD_TX_MULTI)
#define SDMMC_CMD_APP_CMD               (TMIO_CMD_INDEX(55) | TMIO_CMD_RESP_48)

#define SDMMC_ACMD_SET_BUS_WIDTH        (TMIO_CMD_INDEX(6)  | TMIO_CMD_TYPE_ACMD | TMIO_CMD_RESP_48)
#define SDMMC_ACMD_SEND_OP_COND         (TMIO_CMD_INDEX(41) | TMIO_CMD_TYPE_ACMD | TMIO_CMD_RESP_48_NOCRC)
#define SDMMC_ACMD_SET_CLR_CARD_DETECT  (TMIO_CMD_INDEX(42) | TMIO_CMD_TYPE_ACMD | TMIO_CMD_RESP_48)
#define SDMMC_ACMD_GET_SCR              (TMIO_CMD_INDEX(51) | TMIO_CMD_TYPE_ACMD | TMIO_CMD_RESP_48 | TMIO_CMD_TX | TMIO_CMD_TX_READ)

#define SDMMC_CMD_MMC_SWITCH_ARG(_access,_index,_value) \
	((((_value)&0xff)<<8) | (((_index)&0xff)<<16) | (((_access)&3)<<24))

#define SDMMC_SECTOR_SZ 512

MK_EXTERN_C_START

typedef enum SdmmcType {
	SdmmcType_Invalid = 0,
	SdmmcType_MMC,
	SdmmcType_SDv1,
	SdmmcType_SDv2_SDSC,
	SdmmcType_SDv2_SDHC,
} SdmmcType;

typedef struct SdmmcCard {
	TmioCtl* ctl;
	TmioPort port;

	u16 rca;
	SdmmcType type;

	TmioResp cid;
	TmioResp csd;
	u32 ocr;
	u32 scr_hi;
	u32 num_sectors;
} SdmmcCard;

typedef struct SdmmcFrozenState {
	TmioResp cid;
	TmioResp csd;
	u32 ocr;
	u32 scr_hi_be;
	u32 scr_lo_be;
	u16 rca;
	u16 is_mmc;
	u16 is_sdhc;
	u16 is_sd;
	u32 unknown;
	u32 csr;
	u16 tmio_clkctl;
	u16 tmio_option;
	u16 is_ejected;
	u16 tmio_port;
} SdmmcFrozenState;

bool sdmmcCardInit(SdmmcCard* card, TmioCtl* ctl, unsigned port, bool ismmc);
bool sdmmcCardInitFromState(SdmmcCard* card, TmioCtl* ctl, SdmmcFrozenState const* state);
void sdmmcCardDumpState(SdmmcCard* card, SdmmcFrozenState* state);

bool sdmmcCardReadSectors(SdmmcCard* card, TmioTx* tx, u32 sector_id, u32 num_sectors);
bool sdmmcCardWriteSectors(SdmmcCard* card, TmioTx* tx, u32 sector_id, u32 num_sectors);

MK_EXTERN_C_END
