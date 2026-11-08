// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#include "types.h"

#define MWL_MAC_RAM    0x4000
#define MWL_MAC_RAM_SZ (0x2000 - sizeof(MwlMacVars))

#if defined(__NDS__) && defined(ARM7)
#include "../../nds/io.h"

#define MWL_MAC_RAM_ADDR (MM_IO + IO_MITSUMI_WS0 + MWL_MAC_RAM)
#define g_mwlMacVars     ((volatile MwlMacVars*)(MWL_MAC_RAM_ADDR + MWL_MAC_RAM_SZ))
#endif

MK_EXTERN_C_START

typedef enum MwlTxEvent {
	MwlTxEvent_Dropped = 0,
	MwlTxEvent_Queued  = 1,
	MwlTxEvent_Done    = 2,
	MwlTxEvent_Error   = 3,
} MwlTxEvent;

typedef struct MwlMacVars {
	u16 unk_0x00[8];
	u16 unk_0x10;
	u16 unk_0x12;
	u16 unk_0x14;
	u16 unk_0x16;
	u16 unk_0x18;
	u16 unk_0x1a;
	u16 unk_0x1c;
	u16 unk_0x1e;
	u16 wep_keys[4][0x10];
} MwlMacVars;

typedef void (*MwlTxCallback)(void* arg, MwlTxEvent evt, MwlDataTxHdr* hdr);

void mwlDevWakeUp(void);
void mwlDevReset(void);
void mwlDevSetChannel(unsigned ch);
void mwlDevSetMode(MwlMode mode);
void mwlDevSetBssid(const void* bssid);
void mwlDevSetSsid(const char* ssid, unsigned ssid_len);
void mwlDevSetPreamble(bool isShort);
void mwlDevSetAuth(WlanBssAuthType type, WlanAuthData const* data);
void mwlDevShutdown(void);

void mwlDevStart(void);
void mwlDevStop(void);
void mwlDevGracefulStop(void);

bool mwlDevWlanToDix(NetBuf* pPacket);
bool mwlDevDixToWlan(NetBuf* pPacket);

void mwlDevTx(unsigned qid, NetBuf* pPacket, MwlTxCallback cb, void* arg);

MK_EXTERN_C_END
