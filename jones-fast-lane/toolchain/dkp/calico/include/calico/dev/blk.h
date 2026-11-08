// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#include "../types.h"

/*! @addtogroup blkdev
	Provides a common device-independent interface to all available storage devices.
	@{
*/

//! Block device sector size (in bytes)
#define BLK_SECTOR_SZ       512
//! Block device sector size (in words)
#define BLK_SECTOR_SZ_WORDS (BLK_SECTOR_SZ/sizeof(u32))

MK_EXTERN_C_START

//! List of available block devices
typedef enum BlkDevice {

#if defined(__NDS__)

	BlkDevice_Dldi       = 0, //!< DLDI device
	BlkDevice_TwlSdCard  = 1, //!< DSi SD card device
	BlkDevice_TwlNand    = 2, //!< DSi system memory (raw sector access)
	BlkDevice_TwlNandAes = 3, //!< DSi system memory (transparent encryption)

#else
#error "Unsupported platform"
#endif

} BlkDevice;

//! @private
typedef void (*BlkDevCallbackFn)(BlkDevice dev, bool insert);

//! Initializes the block device subsystem
void blkInit(void);

//! @private
void blkSetDevCallback(BlkDevCallbackFn fn);

//! Returns true if block device @p dev is present (and inserted if applicable)
bool blkDevIsPresent(BlkDevice dev);

//! Initializes block device @p dev, returning true on success
bool blkDevInit(BlkDevice dev);

//! Retrieves the size in sectors of block device @p dev
u32  blkDevGetSectorCount(BlkDevice dev);

/*! @brief Reads sectors from block device @p dev
	@param[out] buffer Output buffer (alignment note, see below)
	@param[in] first_sector Index of the first sector to read
	@param[in] num_sectors Number of sectors to read
	@return true on success, false on failure
	@warning On the ARM9, the output buffer must be cache line (32-<b>byte</b>) aligned and in main RAM.
	On the ARM7, the output buffer must be 32-<b>bit</b> aligned.
*/
bool blkDevReadSectors(BlkDevice dev, void* buffer, u32 first_sector, u32 num_sectors);

/*! @brief Writes sectors to block device @p dev
	@param[in] buffer Input buffer (**must be 32-bit aligned**)
	@param[in] first_sector Index of the first sector to write
	@param[in] num_sectors Number of sectors to write
	@return true on success, false on failure
*/
bool blkDevWriteSectors(BlkDevice dev, const void* buffer, u32 first_sector, u32 num_sectors);

MK_EXTERN_C_END

//! @}
