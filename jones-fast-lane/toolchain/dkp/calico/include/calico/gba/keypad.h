// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#include "../types.h"

#if defined(__GBA__)
#include "io.h"
#elif defined(__NDS__)
#include "../nds/io.h"
#ifdef ARM7
#include "../nds/arm7/gpio.h"
#endif
#else
#error "This header file is only for GBA and NDS"
#endif

/*! @addtogroup keypad
	@{
*/

/*! @name Keypad memory mapped I/O
	@{
*/

#define REG_KEYINPUT MK_REG(u16, IO_KEYINPUT)
#define REG_KEYCNT   MK_REG(u16, IO_KEYCNT)

#define KEYCNT_IRQ_ENABLE (1U<<14)
#define KEYCNT_IRQ_OR     (0U<<15)
#define KEYCNT_IRQ_AND    (1U<<15)

//! @}

/*! @name Keypad button bits
	@{
*/

#define KEY_A        (1U<<0)
#define KEY_B        (1U<<1)
#define KEY_SELECT   (1U<<2)
#define KEY_START    (1U<<3)
#define KEY_RIGHT    (1U<<4)
#define KEY_LEFT     (1U<<5)
#define KEY_UP       (1U<<6)
#define KEY_DOWN     (1U<<7)
#define KEY_R        (1U<<8)
#define KEY_L        (1U<<9)
#define KEY_MASK     0x03ff

#if defined(__NDS__)
#define KEY_X        (1U<<10)
#define KEY_Y        (1U<<11)
#define KEY_HINGE    (1U<<12)
#define KEY_DEBUG    (1U<<13)
#define KEY_MASK_EXT 0x3c00
#endif

//! @}

MK_EXTERN_C_START

//! Keypad state object
typedef struct Keypad {
	u16 cur; //!< Currently held keys
	u16 old; //!< Previously held heys
} Keypad;

//! @private
MK_INLINE unsigned keypadGetInState(void)
{
	return ~REG_KEYINPUT & KEY_MASK;
}

#if defined(__GBA__)

//! Retrieves the current state of all keys
MK_INLINE unsigned keypadGetState(void)
{
	return keypadGetInState();
}

#elif defined(__NDS__) && defined(ARM7)

//! @private
MK_INLINE unsigned keypadGetExtState(void)
{
	unsigned state = 0;
	unsigned rcnt_ext = REG_RCNT_EXT;
	state |= (~rcnt_ext & (RCNT_EXT_X|RCNT_EXT_Y|RCNT_EXT_DEBUG)) * KEY_X;
	state |= ( rcnt_ext & RCNT_EXT_HINGE) ? KEY_HINGE : 0U;
	return state;
}

//! Retrieves the current state of all keys
MK_INLINE unsigned keypadGetState(void)
{
	return keypadGetInState() | keypadGetExtState();
}

//! Starts sharing extended key state with the ARM9
void keypadStartExtServer(void);

#else

//! Retrieves the current state of all keys
unsigned keypadGetState(void);

#endif

//! Updates the state of Keypad object @p k
MK_INLINE void keypadRead(Keypad* k)
{
	k->old = k->cur;
	k->cur = keypadGetState();
}

//! Retrieves the bitmask of held keys in @p k
MK_INLINE u16 keypadHeld(Keypad const* k)
{
	return k->cur;
}

//! Retrieves the bitmask of recently pressed keys in @p k
MK_INLINE u16 keypadDown(Keypad const* k)
{
	return ~k->old & k->cur;
}

//! Retrieves the bitmask of recently released keys in @p k
MK_INLINE u16 keypadUp(Keypad const* k)
{
	return k->old & ~k->cur;
}

MK_EXTERN_C_END

//! @}
