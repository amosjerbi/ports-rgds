// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#if !defined(__NDS__)
#error "This header file is only for NDS"
#endif

#include "../types.h"
#include "mm.h"
#include "system.h"

/*! @addtogroup gbacart
	@{
*/

/*! @name GBA cartridge slot timings
	@{
*/

#define GBA_WAIT_SRAM_MASK (3U<<0)
#define GBA_WAIT_SRAM_10   (0U<<0)
#define GBA_WAIT_SRAM_8    (1U<<0)
#define GBA_WAIT_SRAM_6    (2U<<0)
#define GBA_WAIT_SRAM_18   (3U<<0)

#define GBA_WAIT_ROM_N_MASK (3U<<2)
#define GBA_WAIT_ROM_N_10   (0U<<2)
#define GBA_WAIT_ROM_N_8    (1U<<2)
#define GBA_WAIT_ROM_N_6    (2U<<2)
#define GBA_WAIT_ROM_N_18   (3U<<2)

#define GBA_WAIT_ROM_S_MASK (1U<<4)
#define GBA_WAIT_ROM_S_6    (0U<<4)
#define GBA_WAIT_ROM_S_4    (1U<<4)

#define GBA_WAIT_ROM_MASK (GBA_WAIT_ROM_N_MASK|GBA_WAIT_ROM_S_MASK)

#define GBA_PHI_MASK  (3U<<5)
#define GBA_PHI_LOW   (0U<<5)
#define GBA_PHI_4_19  (1U<<5)
#define GBA_PHI_8_38  (2U<<5)
#define GBA_PHI_16_76 (3U<<5)

#define GBA_ALL_MASK (GBA_WAIT_SRAM_MASK|GBA_WAIT_ROM_MASK|GBA_PHI_MASK)

//! @}

MK_EXTERN_C_START

/*! @brief Brokers ownership and access to the GBA cartridge slot from the current CPU
	@return true on success, false if the hardware is in use by the other CPU

	The GBA cartridge slot does not exist on the DSi, and any attempts to open it while
	in DSi mode will fail. In DS mode it will however succeed, although nothing meaningful
	can be done with it since the non-existent slot behaves as open bus (no cartridge inserted).

	On the ARM9, access to the GBA ROM/RAM address space requires ownership of the GBA
	cartridge slot. If the ARM9 does not currently own the slot, attempts to access the
	address space will cause prefetch/data abort exceptions ("segfaults").

	When DLDI drivers for Slot-2 flashcards are loaded, the ARM7 takes ownership
	of the GBA slot; which means calling this function from the ARM9 will fail.
*/
bool gbacartOpen(void);

//! Releases GBA cartridge slot ownership obtained by @ref gbacartOpen
void gbacartClose(void);

//! Returns true if the current CPU owns the GBA cartridge slot
bool gbacartIsOpen(void);

/*! @brief Configures GBA cartridge slot timings
	@param[in] mask Bitmask of timing values to modify
	@param[in] timing New timing values to set
	@return Previous timing values in use
*/
unsigned gbacartSetTiming(unsigned mask, unsigned timing);

MK_EXTERN_C_END

//! @}
