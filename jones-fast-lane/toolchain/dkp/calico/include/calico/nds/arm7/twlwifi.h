// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#if !defined(__NDS__) || !defined(ARM7)
#error "This header file is only for NDS ARM7"
#endif

#include "../../types.h"
#include "../../dev/wlan.h"

MK_EXTERN_C_START

typedef void (*TwlWifiScanCompleteFn)(void* user, WlanBssDesc* bss_list, unsigned bss_count);
typedef void (*TwlWifiAssocFn)(void* user, bool success, unsigned reason);

bool twlwifiInit(void);
void twlwifiExit(void);
bool twlwifiStartScan(WlanBssDesc* out_table, WlanBssScanFilter const* filter, TwlWifiScanCompleteFn cb, void* user);
bool twlwifiIsScanning(void);
bool twlwifiAssociate(WlanBssDesc const* bss, WlanAuthData const* auth, TwlWifiAssocFn cb, void* user);
bool twlwifiDisassociate(void);

bool twlwifiTx(NetBuf* pPacket);

MK_EXTERN_C_END
