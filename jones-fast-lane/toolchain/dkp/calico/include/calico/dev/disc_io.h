// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#include "../types.h"
#include "dldi_defs.h"

/*! @addtogroup blkdev
	@{
*/

/*! @name Generic disc interface
	@{
*/

#define FEATURE_MEDIUM_CANREAD  DLDI_FEATURE_CAN_READ
#define FEATURE_MEDIUM_CANWRITE DLDI_FEATURE_CAN_WRITE
#define FEATURE_SLOT_GBA        DLDI_FEATURE_SLOT_GBA
#define FEATURE_SLOT_NDS        DLDI_FEATURE_SLOT_NDS

MK_EXTERN_C_START

//! @brief Sector offset integer type
typedef u32 sec_t;

typedef bool (* FN_MEDIUM_STARTUP)(void);
typedef bool (* FN_MEDIUM_ISINSERTED)(void);
typedef bool (* FN_MEDIUM_READSECTORS)(sec_t first_sector, sec_t num_sectors, void* buffer);
typedef bool (* FN_MEDIUM_WRITESECTORS)(sec_t first_sector, sec_t num_sectors, const void* buffer);
typedef bool (* FN_MEDIUM_CLEARSTATUS)(void);
typedef bool (* FN_MEDIUM_SHUTDOWN)(void);

//! @brief Generic disc interface struct
typedef struct DISC_INTERFACE_STRUCT {
	u32 ioType;
	u32 features;

	FN_MEDIUM_STARTUP      startup;
	FN_MEDIUM_ISINSERTED   isInserted;
	FN_MEDIUM_READSECTORS  readSectors;
	FN_MEDIUM_WRITESECTORS writeSectors;
	FN_MEDIUM_CLEARSTATUS  clearStatus;
	FN_MEDIUM_SHUTDOWN     shutdown;
} DISC_INTERFACE;

MK_EXTERN_C_END

//! @}

//! @}
