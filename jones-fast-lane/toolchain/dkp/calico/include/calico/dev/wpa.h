// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#include "../system/mailbox.h"
#include "netbuf.h"
#include "wlan.h"

#define WPA_EAPOL_NONCE_LEN  32
#define WPA_EAPOL_MIC_LEN    16
#define WPA_EAPOL_REPLAY_LEN 8
#define WPA_RSC_LEN          8
#define WPA_SHA1_LEN         20
#define WPA_RC4_IV_LEN       16
#define WPA_AES_BLOCK_LEN    16
#define WPA_AES_MAX_ROUNDS   14
#define WPA_AES_WRAP_BLK_LEN 8

MK_EXTERN_C_START

typedef struct WpaAesContext {
	alignas(4) u8 round_keys[1+WPA_AES_MAX_ROUNDS][WPA_AES_BLOCK_LEN];
	u32 num_rounds;
} WpaAesContext;

typedef struct WpaState WpaState;

typedef enum WpaEapolDescrType {
	WpaEapolDescrType_RSN = 0x02,
	WpaEapolDescrType_WPA = 0xfe,
} WpaEapolDescrType;

#define WPA_EAPOL_DESCR_VER(_x)     ((_x)&7)
#define WPA_EAPOL_KEY_TYPE_MASK     (1U<<3)
#define WPA_EAPOL_KEY_TYPE_GROUP    (0U<<3)
#define WPA_EAPOL_KEY_TYPE_PAIRWISE (1U<<3)
#define WPA_EAPOL_OLD_KEY_IDX_MASK  (3U<<4)
#define WPA_EAPOL_OLD_KEY_IDX(_x)   (((_x)>>4)&3)
#define WPA_EAPOL_KEY_INSTALL       (1U<<6)
#define WPA_EAPOL_KEY_ACK           (1U<<7)
#define WPA_EAPOL_KEY_MIC           (1U<<8)
#define WPA_EAPOL_SECURE            (1U<<9)
#define WPA_EAPOL_ERROR             (1U<<10)
#define WPA_EAPOL_REQUEST           (1U<<11)
#define WPA_EAPOL_ENCRYPTED         (1U<<12)

typedef struct WpaEapolHdr {
	u8 protocol_version;
	u8 packet_type;
	u16 packet_body_len_be;
} WpaEapolHdr;

typedef struct __attribute__((packed)) WpaEapolKeyHdr {
	u8 descr_type; // WpaEapolDescrType
	u16 key_info_be;
	u16 key_len_be;
	u8 key_replay_cnt[WPA_EAPOL_REPLAY_LEN];
	u8 key_nonce[WPA_EAPOL_NONCE_LEN];
	u8 key_iv[WPA_RC4_IV_LEN];
	u8 key_rsc[WPA_RSC_LEN];
	u8 reserved[8];

	u8 mic[WPA_EAPOL_MIC_LEN];
	u16 key_data_len_be;
} WpaEapolKeyHdr;

typedef struct WpaKey {
	u8 main[0x10]; // Used for both TKIP and AES-CCMP
	u8 tx[8];      // Used only for TKIP
	u8 rx[8];      // Used only for TKIP
} WpaKey;

typedef struct WpaPtkPad {
	char magic[22];
	u8 zero;
	u8 min_bssid[6];
	u8 max_bssid[6];
	u8 min_nonce[WPA_EAPOL_NONCE_LEN];
	u8 max_nonce[WPA_EAPOL_NONCE_LEN];
	u8 counter;
} WpaPtkPad;

typedef struct WpaPtk {
	u8 kck[0x10]; // Key Confirmation Key (for MIC calculation)
	u8 kek[0x10]; // Key Encryption Key (for key data decryption)
	WpaKey tk;    // Temporal Key (for data packets)
} WpaPtk;

struct WpaState {
	Mailbox mbox;
	u32 mbox_storage;

	NetBuf* (* cb_alloc_packet)(WpaState* st, size_t len);
	void (* cb_tx)(WpaState* st, NetBuf* pPacket);
	void (* cb_install_key)(WpaState* st, bool is_group, unsigned slot, unsigned len);

	// Information element (RSN or WPA)
	void* ie_data;
	unsigned ie_len;

	u8 pmk[WLAN_WPA_PSK_LEN]; // Pairwise Master Key
	WpaPtk ptk; // Pairwise Transient Key
	WpaKey gtk; // Group Transient Key
	u8 rsc[WPA_RSC_LEN]; // Received Sequence Counter

	u64 replay;
	u8 anonce[WPA_EAPOL_NONCE_LEN];
};

void wpaHmacSha1(void* out, const void* key, size_t key_len, const void* data, size_t data_len);
void wpaPseudoRandomFunction(void* out, size_t out_len, const void* key, size_t key_size, void* pad, size_t pad_len);
void wpaGenerateEapolNonce(void* out);

void wpaAesEncrypt(const void* in, void* out, WpaAesContext const* ctx);
void wpaAesDecrypt(const void* in, void* out, WpaAesContext const* ctx);
void wpaAesSetEncryptKey(const void* key, unsigned bits, WpaAesContext* ctx);
void wpaAesSetDecryptKey(const void* key, unsigned bits, WpaAesContext* ctx);
bool wpaAesUnwrap(const void* kek, const void* in, void* out, size_t num_blk);

void wpaPrepare(WpaState* st);
void wpaReset(WpaState* st, const void* pmk);
int wpaSupplicantThreadMain(WpaState* st);

MK_INLINE bool wpaEapolRx(WpaState* st, NetBuf* pPacket)
{
	return mailboxTrySend(&st->mbox, (u32)pPacket);
}

MK_INLINE bool wpaFinalize(WpaState* st)
{
	return mailboxTrySend(&st->mbox, 0);
}

MK_EXTERN_C_END
