// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#include "types.h"

MK_EXTERN_C_START

typedef struct MwlMlmeCallbacks {
	void (* onBssInfo)(WlanBssDesc* bssInfo, WlanBssExtra* bssExtra);
	u32 (* onScanEnd)(void);
	void (* onJoinEnd)(bool ok);
	void (* onAuthEnd)(unsigned status);
	void (* onAssocEnd)(unsigned status);
	void (* onStateLost)(MwlStatus new_class, unsigned reason);

	void (* maData)(NetBuf* pPacket);
} MwlMlmeCallbacks;

MwlMlmeCallbacks* mwlMlmeGetCallbacks(void);
bool mwlMlmeScan(WlanBssScanFilter const* filter, unsigned ch_dwell_time);
bool mwlMlmeJoin(WlanBssDesc const* bssInfo, unsigned timeout);
bool mwlMlmeAuthenticate(unsigned timeout);
bool mwlMlmeAssociate(unsigned timeout, bool fake_cck_rates);
bool mwlMlmeDeauthenticate(void);

MK_EXTERN_C_END
