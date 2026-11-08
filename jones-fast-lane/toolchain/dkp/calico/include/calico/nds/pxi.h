// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#if !defined(__NDS__)
#error "This header file is only for NDS"
#endif

#include "../types.h"
#include "io.h"

/*! @addtogroup pxi
	@{
*/

/*! @name PXI memory mapped I/O
	@{
*/

#define REG_PXI_SYNC MK_REG(u16, IO_PXI_SYNC)
#define REG_PXI_CNT  MK_REG(u32, IO_PXI_CNT)
#define REG_PXI_SEND MK_REG(u32, IO_PXI_SEND)
#define REG_PXI_RECV MK_REG(u32, IO_PXI_RECV)

#define PXI_SYNC_RECV(_n)   ((_n) & 0xF)
#define PXI_SYNC_SEND(_n)   (((_n) & 0xF) << 8)
#define PXI_SYNC_IRQ_SEND   (1U << 13)
#define PXI_SYNC_IRQ_ENABLE (1U << 14)

#define PXI_CNT_SEND_EMPTY      (1U << 0)
#define PXI_CNT_SEND_FULL       (1U << 1)
#define PXI_CNT_SEND_IRQ        (1U << 2)
#define PXI_CNT_SEND_CLEAR      (1U << 3)
#define PXI_CNT_RECV_EMPTY      (1U << 8)
#define PXI_CNT_RECV_FULL       (1U << 9)
#define PXI_CNT_RECV_IRQ        (1U << 10)
#define PXI_CNT_ERROR           (1U << 14)
#define PXI_CNT_ENABLE          (1U << 15)

//! @}

//! Maximum number of words queued in PXI FIFO
#define PXI_FIFO_LEN_WORDS 16

//! Special value indicating abscence of a reply to a PXI message
#define PXI_NO_REPLY UINT32_MAX

// Packet format:
//  Bit    Description
//   0-4    Channel
//   5      Direction (0=request, 1=response)
//   6-31   Immediate (26-bit)

// Extended packet format:
//  Bit    Description
//   0-4    Must be 0x1f (PxiChannel_Extended)
//   5      Direction (0=request, 1=response)
//   6-10   Channel
//   11-15  Number of extra words minus 1
//   16-31  Immediate (16-bit)

MK_EXTERN_C_START

//! List of PXI channels
typedef enum PxiChannel {
	// System channels
	PxiChannel_Rsvd0    = 0,  //!< Reserved for future use
	PxiChannel_Power    = 1,  //!< Reserved for power/environment management
	PxiChannel_BlkDev   = 2,  //!< Reserved for block device access (DLDI, SD, eMMC)
	PxiChannel_WlMgr    = 3,  //!< Reserved for wireless management (DS/Mitsumi, DSi/Atheros)
	PxiChannel_NetBuf   = 4,  //!< Reserved for network interface (packet send/receive)
	PxiChannel_Touch    = 5,  //!< Reserved for touch screen control
	PxiChannel_Sound    = 6,  //!< Reserved for sound hardware access
	PxiChannel_Mic      = 7,  //!< Reserved for microphone access
	PxiChannel_Camera   = 8,  //!< Reserved for DSi camera access
	PxiChannel_Rsvd9    = 9,  //!< Reserved for future use
	PxiChannel_Rsvd10   = 10, //!< Reserved for future use
	PxiChannel_Rsvd11   = 11, //!< Reserved for future use
	PxiChannel_Reset    = 12, //!< Special channel used for ret2hbmenu
	PxiChannel_Rsvd13   = 13, //!< Reserved for future use
	PxiChannel_Rsvd14   = 14, //!< Reserved for future use
	PxiChannel_Rsvd15   = 15, //!< Reserved for future use
	PxiChannel_Rsvd16   = 16, //!< Reserved for future use
	PxiChannel_Rsvd17   = 17, //!< Reserved for future use
	PxiChannel_Rsvd18   = 18, //!< Reserved for future use
	PxiChannel_Rsvd19   = 19, //!< Reserved for future use
	PxiChannel_Rsvd20   = 20, //!< Reserved for future use
	PxiChannel_Rsvd21   = 21, //!< Reserved for future use
	PxiChannel_Rsvd22   = 22, //!< Reserved for future use

	// General purpose user channels
	PxiChannel_User0    = 23, //!< PXI channel available for users
	PxiChannel_User1    = 24, //!< PXI channel available for users
	PxiChannel_User2    = 25, //!< PXI channel available for users
	PxiChannel_User3    = 26, //!< PXI channel available for users
	PxiChannel_User4    = 27, //!< PXI channel available for users
	PxiChannel_User5    = 28, //!< PXI channel available for users
	PxiChannel_User6    = 29, //!< PXI channel available for users
	PxiChannel_User7    = 30, //!< PXI channel available for users

	PxiChannel_Extended = 31, //!< Special channel used to send extended packets
	PxiChannel_Count = PxiChannel_Extended,
} PxiChannel;

//! @private
MK_CONSTEXPR u32 pxiMakePacket(PxiChannel ch, bool dir, u32 imm)
{
	return (ch & 0x1f) | ((unsigned)dir << 5) | (imm << 6);
}

//! @private
MK_CONSTEXPR PxiChannel pxiPacketGetChannel(u32 packet)
{
	return (PxiChannel)(packet & 0x1f);
}

//! @private
MK_CONSTEXPR bool pxiPacketIsResponse(u32 packet)
{
	return (packet >> 5) & 1;
}

//! @private
MK_CONSTEXPR bool pxiPacketIsRequest(u32 packet)
{
	return !pxiPacketIsResponse(packet);
}

//! @private
MK_CONSTEXPR u32 pxiPacketGetImmediate(u32 packet)
{
	return packet >> 6;
}

//! @private
MK_CONSTEXPR u32 pxiMakeExtPacket(PxiChannel ch, bool dir, unsigned num_words, u16 imm)
{
	return PxiChannel_Extended | ((unsigned)dir << 5) | ((ch & 0x1f) << 6) | (((num_words - 1) & 0x1f) << 11) | (imm << 16);
}

//! @private
MK_CONSTEXPR PxiChannel pxiExtPacketGetChannel(u32 packet)
{
	return (PxiChannel)((packet >> 6) & 0x1f);
}

//! @private
MK_CONSTEXPR unsigned pxiExtPacketGetNumWords(u32 packet)
{
	return ((packet >> 11) & 0x1f) + 1;
}

//! @private
MK_CONSTEXPR u16 pxiExtPacketGetImmediate(u32 packet)
{
	return packet >> 16;
}

/*! @brief Handler callback for incoming PXI messages
	@param[in] user User data set by @ref pxiSetHandler
	@param[in] data Data packet
	@warning The handler callback runs in IRQ mode - exercise caution!
	See @ref IrqHandler for more details on how to write IRQ mode handlers.
*/
typedef void (* PxiHandlerFn)(void* user, u32 data);

struct Mailbox; // forward declare

//! Sends a ping to the other CPU. This function is similar to AArch64's `sev` instruction.
MK_INLINE void pxiPing(void)
{
	REG_PXI_SYNC |= PXI_SYNC_IRQ_SEND;
}

//! Wait for the other CPU to ping this CPU. This function is similar to AArch64's `wfe` instruction.
void pxiWaitForPing(void);

//! Sets @p fn as the handler callback for PXI channel @p ch with @p user data
void pxiSetHandler(PxiChannel ch, PxiHandlerFn fn, void* user);

//! Configures PXI channel @p ch to route its messages to the specified Mailbox @p mb
void pxiSetMailbox(PxiChannel ch, struct Mailbox* mb);

//! Waits for the other CPU to set a handler callback or mailbox on PXI channel @p ch
void pxiWaitRemote(PxiChannel ch);

//! @private
void pxiSendPacket(u32 packet);

//! @private
void pxiSendExtPacket(u32 packet, const u32* data);

//! @private
void pxiBeginReceive(PxiChannel ch);

//! @private
u32 pxiEndReceive(PxiChannel ch);

//! Sends a simple 26-bit value @p imm over PXI channel @p ch
MK_INLINE void pxiSend(PxiChannel ch, u32 imm)
{
	pxiSendPacket(pxiMakePacket(ch, false, imm));
}

/*! @brief Sends an extended message over PXI channel @p ch
	@param[in] imm 16-bit immediate data
	@param[in] data Data buffer to send
	@param[in] num_words Size of the data buffer in words
*/
MK_INLINE void pxiSendWithData(PxiChannel ch, u16 imm, const u32* data, u32 num_words)
{
	pxiSendExtPacket(pxiMakeExtPacket(ch, false, num_words, imm), data);
}

//! Replies to a message on PXI channel @p ch using 26-bit value @p imm
MK_INLINE void pxiReply(PxiChannel ch, u32 imm)
{
	pxiSendPacket(pxiMakePacket(ch, true, imm));
}

/*! @brief Sends a message over PXI channel @p ch, and receives its corresponding reply
	@param[in] imm 26-bit value to send
	@return Message reply, sent by the other CPU using @ref pxiReply
*/
MK_INLINE u32 pxiSendAndReceive(PxiChannel ch, u32 imm)
{
	pxiBeginReceive(ch);
	pxiSend(ch, imm);
	return pxiEndReceive(ch);
}

/*! @brief Sends an extended message over PXI channel @p ch, and receives its corresponding reply
	@param[in] imm 16-bit immediate value to send
	@param[in] data Data buffer to send
	@param[in] num_words Size of the data buffer in words
	@return Message reply, sent by the other CPU using @ref pxiReply
*/
MK_INLINE u32 pxiSendWithDataAndReceive(PxiChannel ch, u32 imm, const u32* data, u32 num_words)
{
	pxiBeginReceive(ch);
	pxiSendWithData(ch, imm, data, num_words);
	return pxiEndReceive(ch);
}

MK_EXTERN_C_END

//! @}
