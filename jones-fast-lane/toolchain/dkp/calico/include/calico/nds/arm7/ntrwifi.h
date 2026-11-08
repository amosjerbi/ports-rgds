// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#if !defined(__NDS__) || !defined(ARM7)
#error "This header file is only for NDS ARM7"
#endif

#include "../../types.h"
#include "../../dev/wlan.h"

MK_EXTERN_C_START

typedef void (*NtrWifiScanCompleteFn)(void* user, WlanBssDesc* bss_list, unsigned bss_count);
typedef void (*NtrWifiAssocFn)(void* user, bool success, unsigned reason);

bool ntrwifiInit(void);
void ntrwifiExit(void);
bool ntrwifiStartScan(WlanBssDesc* out_table, WlanBssScanFilter const* filter, NtrWifiScanCompleteFn cb, void* user);
bool ntrwifiAssociate(WlanBssDesc const* bss, WlanAuthData const* auth, NtrWifiAssocFn cb, void* user);
bool ntrwifiDisassociate(void);

bool ntrwifiTx(NetBuf* pPacket);

MK_EXTERN_C_END
