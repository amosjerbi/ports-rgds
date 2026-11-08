// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#if !defined(__NDS__) || !(defined(ARM9) || defined(ARM7))
#error "This header file is only for NDS"
#endif

#include "../types.h"
#include "../system/sysclock.h"
#include "io.h"

/*! @addtogroup scfg
	@{
*/

#define SCFG_MCINSREM_CLKDIV 0x200
#define SCFG_MCINSREM_CLOCK  (SYSTEM_CLOCK/SCFG_MCINSREM_CLKDIV)

/*! @name SCFG memory mapped I/O
	@{
*/

#define REG_SCFG_ROM MK_REG(u32, IO_SCFG_ROM)
#define REG_SCFG_CLK MK_REG(u16, IO_SCFG_CLK)
#define REG_SCFG_EXT MK_REG(u32, IO_SCFG_EXT)
#define REG_SCFG_MC  MK_REG(u16, IO_SCFG_MC)

#define REG_MBK_SLOT_Ax(_x) MK_REG(u8,  IO_MBK_SLOT_Ax(_x)) // arm7 ro, arm9 rw/ro (see SLOTWRPROT)
#define REG_MBK_SLOT_Bx(_x) MK_REG(u8,  IO_MBK_SLOT_Bx(_x))
#define REG_MBK_SLOT_Cx(_x) MK_REG(u8,  IO_MBK_SLOT_Cx(_x))
#define REG_MBK_MAP_A       MK_REG(u32, IO_MBK_MAP_A)       // arm7 rw, arm9 rw (per-cpu)
#define REG_MBK_MAP_B       MK_REG(u32, IO_MBK_MAP_B)
#define REG_MBK_MAP_C       MK_REG(u32, IO_MBK_MAP_C)
#define REG_MBK_SLOTWRPROT  MK_REG(u32, IO_MBK_SLOTWRPROT)  // arm7 rw, arm9 ro

#ifdef ARM7
#define REG_SCFG_A9ROM MK_REG(u8,  IO_SCFG_ROM+0)
#define REG_SCFG_A7ROM MK_REG(u8,  IO_SCFG_ROM+1)
#define REG_SCFG_RST   MK_REG(u16, IO_SCFG_RST)
#define REG_SCFG_MCINS MK_REG(u16, IO_SCFG_MCINS)
#define REG_SCFG_MCREM MK_REG(u16, IO_SCFG_MCREM)
#define REG_SCFG_WL    MK_REG(u8,  IO_SCFG_WL)
#define REG_SCFG_OP    MK_REG(u8,  IO_SCFG_OP)

#define REG_OTP_CID    MK_REG(u64, IO_OTP_CID)
#define REG_OTP_ERROR  MK_REG(u8,  IO_OTP_ERROR)
#endif

#ifdef ARM9
#define REG_SCFG_JTAG MK_REG(u16, IO_SCFG_JTAG)
#endif

//! @}

/*! @name REG_SCFG_ROM bits
	@{
*/

#define SCFG_ROM_MASK        (3U<<0)
#define SCFG_ROM_BROM_PROT   (1U<<0)
#define SCFG_ROM_MODE_TWL    (0U<<1)
#define SCFG_ROM_MODE_NTR    (1U<<1)

#define SCFG_ROM_A9MASK      SCFG_ROM_MASK
#define SCFG_ROM_A9BROM_PROT SCFG_ROM_BROM_PROT
#define SCFG_ROM_A9MODE_TWL  SCFG_ROM_MODE_TWL
#define SCFG_ROM_A9MODE_NTR  SCFG_ROM_MODE_NTR

#ifdef ARM7
#define SCFG_ROM_A7MASK      (SCFG_ROM_MASK<<8)
#define SCFG_ROM_A7BROM_PROT (SCFG_ROM_BROM_PROT<<8)
#define SCFG_ROM_A7MODE_TWL  (SCFG_ROM_MODE_TWL<<8)
#define SCFG_ROM_A7MODE_NTR  (SCFG_ROM_MODE_NTR<<8)
#define SCFG_ROM_OTP_PROT    (1U<<10)
#endif

//! @}

/*! @name REG_SCFG_CLK bits
	@{
*/

#ifdef ARM7
#define SCFG_CLK_TMIO0       (1U<<0)
#define SCFG_CLK_UNK1        (1U<<1)
#define SCFG_CLK_AES         (1U<<2)
#define SCFG_CLK_NWRAM       (1U<<7)
#define SCFG_CLK_CODEC       (1U<<8)
#endif

#ifdef ARM9
#define SCFG_CLK_CPU_67MHz   (0U<<0)
#define SCFG_CLK_CPU_134MHz  (1U<<0)
#define SCFG_CLK_DSP         (1U<<1)
#define SCFG_CLK_CAM_IFACE   (1U<<2)
#define SCFG_CLK_NWRAM       (1U<<7) // read-only
#define SCFG_CLK_CAM_EXT     (1U<<8)
#endif

//! @}

/*! @name REG_SCFG_EXT bits
	@{
*/

#define SCFG_EXT_FIX_DMA   (1U<<0)  // arm9 rw, arm7 rw (per-cpu)
#define SCFG_EXT_FIX_CARD  (1U<<7)  // arm9 rw, arm7 ro
#define SCFG_EXT_TWL_IRQ   (1U<<8)  // arm9 rw, arm7 rw (per-cpu)
#define SCFG_EXT_TWL_LCD   (1U<<12) // arm9 rw, arm7 ro
#define SCFG_EXT_TWL_VRAM  (1U<<13) // arm9 rw, arm7 ro
#define SCFG_EXT_RAM_MASK  (3U<<14) // arm9 rw, arm7 ro
#define SCFG_EXT_RAM_4MB   (0U<<14)
#define SCFG_EXT_RAM_16MB  (2U<<14)
#define SCFG_EXT_RAM_32MB  (3U<<14)
#define SCFG_EXT_HAS_NDMA  (1U<<16) // arm9 rw, arm7 rw (per-cpu)
#define SCFG_EXT_HAS_CARD2 (1U<<24) // arm9 ro, arm7 rw
#define SCFG_EXT_HAS_NWRAM (1U<<25) // arm9 ro, arm7 rw
#define SCFG_EXT_HAS_SCFG  (1U<<31) // arm9 rw, arm7 rw (per-cpu)

#define SCFG_EXT7_FIX_SND_DMA (1U<<1)
#define SCFG_EXT7_FIX_SND     (1U<<2)
#define SCFG_EXT7_TWL_SPI     (1U<<9)
#define SCFG_EXT7_TWL_SND_DMA (1U<<10)
#define SCFG_EXT7_TWL_UNK11   (1U<<11)
#define SCFG_EXT7_HAS_AES     (1U<<17)
#define SCFG_EXT7_HAS_TMIO0   (1U<<18)
#define SCFG_EXT7_HAS_TMIO1   (1U<<19)
#define SCFG_EXT7_HAS_MICEX   (1U<<20)
#define SCFG_EXT7_HAS_SNDEX   (1U<<21)
#define SCFG_EXT7_HAS_I2C     (1U<<22)
#define SCFG_EXT7_HAS_GPIO    (1U<<23)
#define SCFG_EXT7_HAS_UNK28   (1U<<28)

#define SCFG_EXT9_FIX_3D_GEO  (1U<<1)
#define SCFG_EXT9_FIX_3D_REN  (1U<<2)
#define SCFG_EXT9_FIX_2D      (1U<<3)
#define SCFG_EXT9_FIX_DIV     (1U<<4)
#define SCFG_EXT9_HAS_CAM     (1U<<17)
#define SCFG_EXT9_HAS_DSP     (1U<<18)

//! @}

/*! @name REG_SCFG_MC bits
	@{
*/

#define SCFG_MC_IS_EJECTED    (1U<<0)
#define SCFG_MC_POWER_MASK    (3U<<2)
#define SCFG_MC_POWER_OFF     (0U<<2)
#define SCFG_MC_POWER_ON_REQ  (1U<<2)
#define SCFG_MC_POWER_ON      (2U<<2)
#define SCFG_MC_POWER_OFF_REQ (3U<<2)

//! @}

/*! @name REG_MBK_SLOT bits
	@{
*/

#define MBK_SLOT_OWNER(_n)  ((_n)&3)
#define MBK_SLOT_OFFSET(_n) (((_n)&3)<<2)
#define MBK_SLOT_ENABLE     (1U<<7)

//! @}

/*! @name REG_MBK_MAP bits
	@{
*/

#define MBK_MAP_START(_n) (((_n)&0x1ff)<<3)
#define MBK_MAP_SIZE(_n)  (((_n)&3)<<12)
#define MBK_MAP_END(_n)   (((_n)&0x3ff)<<19)

#define MBK_MAP_GRANULARITY 0x8000

//! @}

//! SCFG register backup on shared main RAM
#define g_scfgBackup ((ScfgBackup*)MM_ENV_TWL_SCFG_BACKUP)

MK_EXTERN_C_START

//! New WRAM slot owner, for use with MBK_SLOT_OWNER
typedef enum MbkSlotOwner {
	MbkSlotOwner_Arm9 = 0,
	MbkSlotOwner_Arm7 = 1,
	MbkSlotOwner_Dsp  = 2, // only for WRAM_B/C
} MbkSlotOwner;

//! New WRAM map size, for use with MBK_MAP_SIZE
typedef enum MbkMapSize {
	MbkMapSize_32K  = 0, // only for WRAM_B/C
	MbkMapSize_64K  = 1,
	MbkMapSize_128K = 2,
	MbkMapSize_256K = 3,
} MbkMapSize;

//! Structure containing backups of SCFG registers
typedef struct ScfgBackup {
	u32 ext;
	union {
		u16 other;
		struct {
			u16 op    : 2;
			u16 a9rom : 2;
			u16 a7rom : 3;
			u16 wl    : 1;
			u16 jtag  : 3;
			u16 clk   : 5;
		};
	};
} ScfgBackup;

#ifdef ARM7
//! SCFG register backup on ARM7 WRAM
extern ScfgBackup __scfg_buf;
#endif

//! Converts @p ms to a value suitable for REG_SCFG_MCINS and REG_SCFG_MCREM
MK_CONSTEXPR u16 scfgCalcMcInsRem(unsigned ms)
{
	return (SCFG_MCINSREM_CLOCK * ms + 500U) / 1000U;
}

//! Returns true if SCFG is available
MK_INLINE bool scfgIsPresent(void)
{
	return (REG_SCFG_EXT & SCFG_EXT_HAS_SCFG) != 0;
}

/*! @brief Calculates a configuration value for the REG_SCFG_SLOT registers
	@param[in] owner Owner of the slot (see @ref MbkSlotOwner)
	@param[in] offset WRAM bank index (0..3 for WRAM A, 0..7 for WRAM B/C)
*/
MK_INLINE u8 mbkMakeSlot(MbkSlotOwner owner, unsigned offset)
{
	return MBK_SLOT_OWNER(owner) | MBK_SLOT_OFFSET(offset) | MBK_SLOT_ENABLE;
}

/*! @brief Calculates a configuration value for the REG_SCFG_MAP registers
	@param[in] start_addr Start of the address range to map
	@param[in] end_addr End of the address range to map (exclusive)
	@param[in] sz Image size of the bank (usually MbkMapSize_256K)
	@note The address range must be contained within the MM_TWLWRAM_MAP area.
	WRAM B/C addresses must be a multiple of `MBK_MAP_GRANULARITY`, while
	WRAM A addresses must be a multiple of `2*MBK_MAP_GRANULARITY`.
*/
MK_INLINE u32 mbkMakeMapping(uptr start_addr, uptr end_addr, MbkMapSize sz)
{
	unsigned start_off = (start_addr - MM_TWLWRAM_MAP) / MBK_MAP_GRANULARITY;
	unsigned end_off   = (end_addr   - MM_TWLWRAM_MAP) / MBK_MAP_GRANULARITY;
	return MBK_MAP_START(start_off) | MBK_MAP_SIZE(sz) | MBK_MAP_END(end_off);
}

/*! @brief Controls the power state of the DS card slot
	@param[in] on true to power on, false to power off
	@return true on success, false if the ARM7 does not have permission to use SCFG
*/
bool scfgSetMcPower(bool on);

MK_EXTERN_C_END

//! @}
