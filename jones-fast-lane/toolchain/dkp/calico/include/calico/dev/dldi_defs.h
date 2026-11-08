// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once

/*! @addtogroup blkdev
	@{
*/

/*! @name DLDI constants
	@{
*/

#define DLDI_MAGIC_VAL         0xbf8da5ed
#define DLDI_MAGIC_STRING      " Chishm"

#define DLDI_MAGIC_STRING_LEN  8
#define DLDI_FRIENDLY_NAME_LEN 48

#define DLDI_FIX_ALL           (1U<<0)
#define DLDI_FIX_GLUE          (1U<<1)
#define DLDI_FIX_GOT           (1U<<2)
#define DLDI_FIX_BSS           (1U<<3)

#define DLDI_FEATURE_CAN_READ  (1U<<0)
#define DLDI_FEATURE_CAN_WRITE (1U<<1)
#define DLDI_FEATURE_SLOT_GBA  (1U<<4)
#define DLDI_FEATURE_SLOT_NDS  (1U<<5)

#define DLDI_SIZE_MAX          DLDI_SIZE_16KB
#define DLDI_SIZE_16KB         14
#define DLDI_SIZE_8KB          13
#define DLDI_SIZE_4KB          12
#define DLDI_SIZE_2KB          11
#define DLDI_SIZE_1KB          10

#define DLDI_MAX_ALLOC_SZ      (1U<<DLDI_SIZE_MAX)

//! @}

//! @}
