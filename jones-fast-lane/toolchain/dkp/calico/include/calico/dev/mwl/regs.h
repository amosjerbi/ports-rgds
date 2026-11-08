// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#include "types.h"

#if defined(__NDS__) && defined(ARM7)
#include "../../nds/io.h"

#define MWL_REG(_off) MK_REG(u16, IO_MITSUMI_WS1+(_off))
#endif

// XX: Using gbatek's register names. Even though they aren't exactly great,
// we use them because they are well known, therefore preserving familiarity
// and improving code readability. Real names are still currently unknown.

#define W_ID            0x000
#define W_MODE_RST      0x004
#define W_MODE_WEP      0x006
#define W_TXSTATCNT     0x008
#define W_IF            0x010
#define W_IE            0x012
#define W_MACADDR_0     0x018
#define W_MACADDR_1     0x01a
#define W_MACADDR_2     0x01c
#define W_BSSID_0       0x020
#define W_BSSID_1       0x022
#define W_BSSID_2       0x024
#define W_AID_LOW       0x028
#define W_AID_FULL      0x02a
#define W_TX_RETRYLIMIT 0x02c
#define W_RXCNT         0x030
#define W_WEP_CNT       0x032
#define W_POWER_US      0x036
#define W_POWER_TX      0x038
#define W_POWERSTATE    0x03c
#define W_POWERFORCE    0x040
#define W_RANDOM        0x044
#define W_RXBUF_BEGIN   0x050
#define W_RXBUF_END     0x052
#define W_RXBUF_WRCSR   0x054
#define W_RXBUF_WR_ADDR 0x056
#define W_RXBUF_RD_ADDR 0x058
#define W_RXBUF_READCSR 0x05a
#define W_RXBUF_COUNT   0x05c
#define W_RXBUF_RD_DATA 0x060
#define W_RXBUF_GAP     0x062
#define W_RXBUF_GAPDISP 0x064
#define W_TXBUF_WR_ADDR 0x068
#define W_TXBUF_COUNT   0x06c
#define W_TXBUF_WR_DATA 0x070
#define W_TXBUF_GAP     0x074
#define W_TXBUF_GAPDISP 0x076
#define W_TXBUF_BEACON  0x080
#define W_TXBUF_TIM     0x084
#define W_LISTENCOUNT   0x088
#define W_BEACONINT     0x08c
#define W_LISTENINT     0x08e
#define W_TXBUF_CMD     0x090
#define W_TXBUF_REPLY1  0x094
#define W_TXBUF_REPLY2  0x098
#define W_TXBUF_LOC1    0x0a0
#define W_TXBUF_LOC2    0x0a4
#define W_TXBUF_LOC3    0x0a8
#define W_TXREQ_RESET   0x0ac
#define W_TXREQ_SET     0x0ae
#define W_TXREQ_READ    0x0b0
#define W_TXBUF_RESET   0x0b4
#define W_TXBUSY        0x0b6
#define W_TXSTAT        0x0b8
#define W_PREAMBLE      0x0bc
#define W_CMD_TOTALTIME 0x0c0
#define W_CMD_REPLYTIME 0x0c4
#define W_RXFILTER      0x0d0
#define W_RX_LEN_CROP   0x0da
#define W_RXFILTER2     0x0e0
#define W_US_COUNTCNT   0x0e8
#define W_US_COMPARECNT 0x0ea
#define W_CMD_COUNTCNT  0x0ee
#define W_US_COMPARE0   0x0f0
#define W_US_COMPARE1   0x0f2
#define W_US_COMPARE2   0x0f4
#define W_US_COMPARE3   0x0f6
#define W_US_COUNT0     0x0f8
#define W_US_COUNT1     0x0fa
#define W_US_COUNT2     0x0fc
#define W_US_COUNT3     0x0fe
#define W_CONTENTFREE   0x10c
#define W_PRE_BEACON    0x110
#define W_CMD_COUNT     0x118
#define W_BEACON_COUNT  0x11c
#define W_POST_BEACON   0x134
#define W_BB_CNT        0x158
#define W_BB_WRITE      0x15a
#define W_BB_READ       0x15c
#define W_BB_BUSY       0x15e
#define W_BB_MODE       0x160
#define W_BB_POWER      0x168
#define W_RF_DATA2      0x17c
#define W_RF_DATA1      0x17e
#define W_RF_BUSY       0x180
#define W_RF_CNT        0x184
#define W_TX_HDR_CNT    0x194
#define W_RF_PINS       0x19c
#define W_RXSTAT_INC_IF 0x1a8
#define W_RXSTAT_INC_IE 0x1aa
#define W_RXSTAT_OVF_IF 0x1ac
#define W_RXSTAT_OVF_IE 0x1ae
#define W_TX_ERR_COUNT  0x1c0
#define W_RX_COUNT      0x1c4
#define W_TX_SEQNO      0x210
#define W_RF_STATUS     0x214
#define W_IF_SET        0x21c
#define W_RAM_DISABLE   0x220
#define W_RXTX_ADDR     0x268

// Interrupt flags
#define W_IRQ_RX_END     (1U<<0)
#define W_IRQ_TX_END     (1U<<1)
#define W_IRQ_RX_CNT_INC (1U<<2)
#define W_IRQ_TX_ERR     (1U<<3)
#define W_IRQ_RX_CNT_OVF (1U<<4)
#define W_IRQ_TX_CNT_OVF (1U<<5)
#define W_IRQ_RX_START   (1U<<6)
#define W_IRQ_TX_START   (1U<<7)
#define W_IRQ_TX_COUNT   (1U<<8)
#define W_IRQ_RX_COUNT   (1U<<9)
#define W_IRQ_RF_WAKEUP  (1U<<11)
#define W_IRQ_MP_END     (1U<<12)
#define W_IRQ_POST_TBTT  (1U<<13)
#define W_IRQ_TBTT       (1U<<14)
#define W_IRQ_PRE_TBTT   (1U<<15)
