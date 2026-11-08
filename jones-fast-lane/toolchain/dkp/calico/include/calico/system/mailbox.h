// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#include "../types.h"
#include "thread.h"

/*! @addtogroup sync
	@{
*/
/*! @name Mailbox
	Synchronization primitive that provides a way to transfer messages between
	threads. Messages are delived in the same order they are sent (i.e. FIFO).
	@{
*/

MK_EXTERN_C_START

//! Mailbox object
typedef struct Mailbox {
	u32* slots;          //!< @private
	u8 num_slots;        //!< @private
	u8 cur_slot;         //!< @private
	u8 pending_slots;    //!< @private
	u8 send_waiters : 4; //!< @private
	u8 recv_waiters : 4; //!< @private
} Mailbox;

/*! @brief Prepares a Mailbox object @p mb for use
	@param[in] slots Storage space for messages
	@param[in] num_slots Capacity of the storage space in words
	@note The storage space must remain valid throughout the lifetime of the Mailbox object.
*/
MK_INLINE void mailboxPrepare(Mailbox* mb, u32* slots, unsigned num_slots)
{
	mb->slots = slots;
	mb->num_slots = num_slots;
	mb->cur_slot = 0;
	mb->pending_slots = 0;
	mb->send_waiters = 0;
	mb->recv_waiters = 0;
}

//! @brief Asynchronously sends a @p message to Mailbox @p mb.
//! Returns true on success, false when the mailbox is full.
bool mailboxTrySend(Mailbox* mb, u32 message);

//! @brief Asynchronously receives a message from Mailbox @p mb.
bool mailboxTryRecv(Mailbox* mb, u32* out);

//! @brief Receives a message from Mailbox @p mb, blocking the current thread if empty.
u32 mailboxRecv(Mailbox* mb);

MK_EXTERN_C_END

//! @}

//! @}
