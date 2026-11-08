// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#include "dldi_defs.h"
#include "disc_io.h"

/*! @addtogroup blkdev
	@{
*/

MK_EXTERN_C_START

//! @brief DLDI driver header
typedef struct DldiHeader {
	u32  magic_num;
	char magic_str[DLDI_MAGIC_STRING_LEN];
	u8   version_num;
	u8   driver_sz_log2;
	u8   fix_flags;
	u8   alloc_sz_log2;

	char iface_name[DLDI_FRIENDLY_NAME_LEN];

	uptr dldi_start;
	uptr dldi_end;
	uptr glue_start;
	uptr glue_end;
	uptr got_start;
	uptr got_end;
	uptr bss_start;
	uptr bss_end;

	DISC_INTERFACE disc;
} DldiHeader;

#if defined(__NDS__) && defined(ARM9)
//! @private
bool dldiDumpInternal(void* buffer);
#endif

MK_EXTERN_C_END

//! @}
