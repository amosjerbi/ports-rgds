// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#if !defined(__NDS__)
#error "This header file is only for NDS"
#endif

/*! @addtogroup mm_env
	@{
*/
/*! @name Shared areas in lower main RAM (DSi-only)
	@{
*/

//! Optional parameters for DSi autoload
#define MM_ENV_TWL_AUTOLOAD_PARAM 0x2000000

//! DSi autoload
#define MM_ENV_TWL_AUTOLOAD       0x2000300

//! DSi user settings (loaded from NAND)
#define MM_ENV_TWL_USER_SETTINGS  0x2000400

//! DSi Atheros wireless firmware information
#define MM_ENV_TWL_WLFIRM_INFO    0x20005e0

//! DSi hardware info (sourced from HWINFO_N.dat)
#define MM_ENV_TWL_HWINFO_N       0x2000600

//! Extended DSi autoload, implemented by some homebrew entrypoints
#define MM_ENV_TWL_AUTOLOAD_EXT   0x2000800

//! Extended DSi settings, implemented by some homebrew entrypoints
#define MM_ENV_TWL_SETTINGS_EXT   0x2000c00

//! Free area reserved for internal use
#define MM_ENV_TWL_FREE_1000      0x2001000
#define MM_ENV_TWL_FREE_1000_SZ   0x3000 //!< Size of @ref MM_ENV_TWL_FREE_1000 (12kb)

//! Start of loadable application memory in DSi mode
#define MM_ENV_TWL_APP_START      0x2004000

//! @}

/*! @name Shared areas in upper main RAM
	@{
*/

//! Homebrew bootstub area, used to implement return-to-hbmenu
#define MM_ENV_HB_BOOTSTUB        0x2ff4000
#define MM_ENV_HB_BOOTSTUB_SZ     0x8000 //!< Size of @ref MM_ENV_HB_BOOTSTUB (32kb)

//! DSi ROM header for the currently inserted gamecard
#define MM_ENV_CARD_TWL_HEADER    0x2ffc000

//! Free area reserved for internal use
#define MM_ENV_FREE_D000          0x2ffd000
#define MM_ENV_FREE_D000_SZ       0x7b0 //!< Size of @ref MM_ENV_FREE_D000

//! DSi system software version information
#define MM_ENV_TWL_VERSION_INFO   0x2ffd7b0

//! DSi eMMC information
#define MM_ENV_TWL_NAND_INFO      0x2ffd7bc

//! DSi known title list
#define MM_ENV_TWL_TITLE_LIST     0x2ffd800

//! DSi device mount list
#define MM_ENV_TWL_DEVICE_LIST    0x2ffdc00

//! DSi ROM header for the currently running application
#define MM_ENV_APP_TWL_HEADER     0x2ffe000

//! Free area reserved for internal use
#define MM_ENV_FREE_F000          0x2fff000
#define MM_ENV_FREE_F000_SZ       0xa80 //!< Size of @ref MM_ENV_FREE_F000 (2.625kb)

//! NDS ROM header for the currently inserted gamecard
#define MM_ENV_CARD_NDS_HEADER    0x2fffa80

//! More parameters used by DS Download Play
#define MM_ENV_BOOT_PARAM_EX      0x2fffbe0
#define MM_ENV_BOOT_PARAM_EX_SZ   0x20 //!< Size of @ref MM_ENV_BOOT_PARAM_EX_SZ

//! Information filled in by the firmware during boot
#define MM_ENV_FW_BOOT_INFO       0x2fffc00

//! Application boot indicator + parameters used by DS Download Play
#define MM_ENV_BOOT_PARAM         0x2fffc40
#define MM_ENV_BOOT_PARAM_SZ      0x40 //!< Size of @ref MM_ENV_BOOT_PARAM

//! User settings (loaded from NVRAM)
#define MM_ENV_USER_SETTINGS      0x2fffc80

//! Free area reserved for internal use
#define MM_ENV_FREE_FCF0          0x2fffcf0
#define MM_ENV_FREE_FCF0_SZ       0x78 //!< Size of @ref MM_ENV_FREE_FCF0

//! DSi hardware info (sourced from HWINFO_S.dat)
#define MM_ENV_TWL_HWINFO_S       0x2fffd68

// ARM9 exception stack + vector
#define MM_ENV_EXCPT_STACK_BOTTOM 0x2fffd80 //!< Bottom of ARM9 exception stack
#define MM_ENV_EXCPT_STACK_TOP    0x2fffd9c //!< Top of ARM9 exception stack
#define MM_ENV_EXCPT_VECTOR       MM_ENV_EXCPT_STACK_TOP //!< Pointer to ARM9 exception handling routine

//! Free area reserved for internal use
#define MM_ENV_FREE_FDA0          0x2fffda0
#define MM_ENV_FREE_FDA0_SZ       0x50 //!< Size of @ref MM_ENV_FREE_FDA0

//! SCFG registers backup
#define MM_ENV_TWL_SCFG_BACKUP    0x2fffdf0

//! DSi reset-related flags (read from McuReg_WarmbootFlag)
#define MM_ENV_TWL_RESET_FLAGS    0x2fffdfa

//! Pointer to DSi settings (usually MM_ENV_TWL_USER_SETTINGS)
#define MM_ENV_TWL_SETTINGS_PTR   0x2fffdfc

//! NDS ROM header for the currently running application
#define MM_ENV_APP_NDS_HEADER     0x2fffe00

//! Argument header
#define MM_ENV_ARGV_HEADER        0x2fffe70

//! Free area reserved for internal use
#define MM_ENV_FREE_FF60          0x2ffff60
#define MM_ENV_FREE_FF60_SZ       0x9c //!< Size of @ref MM_ENV_FREE_FF60

//! Special address used as an I/O port to control the main RAM chip
#define MM_ENV_MAIN_MEM_CMD       0x2fffffe

//! @}

//! @}
