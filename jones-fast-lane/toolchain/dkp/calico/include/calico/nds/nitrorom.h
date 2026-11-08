// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#include "../types.h"
#include "env.h"

/*! @addtogroup nitrorom

	This API provides access to the NitroROM/NitroARC filesystem format, used
	to embed game resources and other assets within a DS ROM. See
	https://problemkaputt.de/gbatek-ds-cartridge-nitrorom-and-nitroarc-file-systems.htm
	for more details.

	File/directory IDs with a numerical value lower than @ref NITROROM_ROOT_DIR always
	correspond to files, while IDs equal to or greater than @ref NITROROM_ROOT_DIR
	always correspond to directories.

	In addition, using libdvm it is possible to use standard file I/O functions instead
	of this API for maximum portability and compatibility with other platforms. Please
	refer to libdvm for more details.

	@{
*/

//! ID of the first (root) directory inside NitroFS
#define NITROROM_ROOT_DIR 0xf000
//! Maximum length in bytes of a NitroFS file/directory name
#define NITROROM_NAME_MAX 0x7f

MK_EXTERN_C_START

// Forward declarations
typedef struct NitroRom          NitroRom;
typedef struct NitroRomParams    NitroRomParams;
typedef struct NitroRomIface     NitroRomIface;
typedef struct NitroRomIter      NitroRomIter;
typedef struct NitroRomIterEntry NitroRomIterEntry;

//! DS ROM file table entry
typedef EnvNdsFileTableEntry     NitroRomFile;
//! DS ROM directory table entry
typedef EnvNdsDirTableEntry      NitroRomDir;

//! DS ROM filesystem parameters
struct NitroRomParams {
	u32 fat_offset; //!< Offset to the start of the FAT region (File Allocation Table)
	u32 fat_sz;     //!< Size of the FAT region
	u32 fnt_offset; //!< Offset to the start of the FNT region (File Name Table)
	u32 fnt_sz;     //!< Size of the FNT region
	u32 img_offset; //!< Offset to the start of the file data region (usually 0)
};

//! DS ROM filesystem object
struct NitroRom {
	const NitroRomIface* iface; //!< @private
	void* user;                 //!< @private

	u32 fnt_offset; //!< @private
	u32 img_offset; //!< @private
	u16 num_files;  //!< @private
	u16 num_dirs;   //!< @private

	NitroRomFile* file_table; //!< @private
	NitroRomDir*  dir_table;  //!< @private
};

//! Interface for reading from a DS ROM
struct NitroRomIface {
	bool (*read)(void* user, u32 offset, void* buf, u32 size);
	void (*close)(void* user);
};

//! DS ROM filesystem directory iterator
struct NitroRomIter {
	NitroRom* nr;      //!< @private
	u32 start;         //!< @private
	u32 cursor;        //!< @private
	u16 first_file_id; //!< @private
	u16 cur_file_id;   //!< @private
};

//! Entry data returned by a DS ROM filesystem directory iterator
struct NitroRomIterEntry {
	char name[NITROROM_NAME_MAX+1]; //!< Name of the file or directory (NUL-terminated)
	u16  id;       //!< ID of the file/directory
	u16  name_len; //!< Length of the name in bytes
};

#ifdef ARM9
/*! @brief Obtains access to the DS ROM filesystem of the current application

	This function may be called multiple times. During the first call, Calico
	will try to find the correct mechanism to access the application's own ROM
	data. Namely, if `argv[0]` was provided to the application, Calico will use
	standard I/O routines to open and read the specified file. <b>Make sure to
	initialize an appropriate filesystem driver such as libdvm</b> before
	attempting to call this function. If `argv[0]` is not provided and the
	application was booted from Slot-1 (see @ref EnvBootSrc_Card), Calico will
	use @ref ntrcard routines to directly read the card instead. This is useful
	when testing the application on emulators.

	@return DS ROM filesystem object on success. If opening the DS ROM failed,
	or if the ROM contains no filesystem, NULL is returned.
*/
NitroRom* nitroromGetSelf(void);
#endif

/*! @brief Opens a DS ROM filesystem @p nr
	@param[in] params See @ref NitroRomParams
	@param[in] iface See @ref NitroRomIface
	@param[in] user User data to pass to the interface
	@return true on success, false on failure
*/
bool nitroromOpen(NitroRom* nr, const NitroRomParams* params, const NitroRomIface* iface, void* user);

//! Closes a DS ROM filesystem @p nr
void nitroromClose(NitroRom* nr);

/*! @brief Reads data from DS ROM @p nr
	@param[in] offset Offset from the start of the ROM
	@param[out] buf Output buffer
	@param[in] size Size in bytes to read
	@return true on success, false on failure
*/
MK_INLINE bool nitroromRead(NitroRom* nr, u32 offset, void* buf, u32 size)
{
	return nr->iface->read(nr->user, offset, buf, size);
}

//! Returns the ROM offset of the file @p file_id within DS ROM filesystem @p nr
MK_INLINE u32 nitroromGetFileOffset(NitroRom* nr, u16 file_id)
{
	return nr->img_offset + nr->file_table[file_id].start_offset;
}

//! Returns the size of the file @p file_id within DS ROM filesystem @p nr
MK_INLINE u32 nitroromGetFileSize(NitroRom* nr, u16 file_id)
{
	EnvNdsFileTableEntry* f = &nr->file_table[file_id];
	return f->end_offset - f->start_offset;
}

/*! @brief Reads data from a file within DS ROM filesystem @p nr
	@param[in] file_id ID of the file
	@param[in] offset Offset within the file in bytes
	@param[out] buf Output buffer
	@param[in] size Size in bytes to read
	@return true on success, false on failure
*/
MK_INLINE bool nitroromReadFile(NitroRom* nr, u16 file_id, u32 offset, void* buf, u32 size)
{
	return nitroromRead(nr, nitroromGetFileOffset(nr, file_id) + offset, buf, size);
}

//! Retrieves the ID of the parent directory of @p dir_id within DS ROM filesystem @p nr
MK_INLINE u16 nitroromGetParentDir(NitroRom* nr, u16 dir_id)
{
	return nr->dir_table[dir_id-NITROROM_ROOT_DIR].parent_id;
}

/*! @brief Opens a directory iterator on the DS ROM filesystem @p nr
	@param[in] dir_id ID of the directory to open
	@param[out] iter Iterator object
*/
MK_INLINE void nitroromOpenIter(NitroRom* nr, u16 dir_id, NitroRomIter* iter)
{
	NitroRomDir* d      = &nr->dir_table[dir_id-NITROROM_ROOT_DIR];
	iter->nr            = nr;
	iter->start         = nr->fnt_offset + d->subtable_offset;
	iter->cursor        = iter->start;
	iter->first_file_id = d->file_id_base;
	iter->cur_file_id   = iter->first_file_id;
}

/*! @brief Reads an entry from the given DS ROM directory iterator @p iter
	@param[out] entry Output entry data, representing a file or directory
	@return true on success, false when reaching the end of the directory
*/
bool nitroromReadIter(NitroRomIter* iter, NitroRomIterEntry* entry);

//! Rewinds DS ROM filesystem iterator @p iter to the beginning of the directory
MK_INLINE void nitroromRewindIter(NitroRomIter* iter)
{
	iter->cursor      = iter->start;
	iter->cur_file_id = iter->first_file_id;
}

/*! @brief Resolves the specified @p path to a file/directory within DS ROM filesystem @p nr
	@param[in] base_dir ID of the base directory (pass @ref NITROROM_ROOT_DIR to search from the root)
	@param[in] path File/directory path, relative to the given base directory
	@return ID of the file/directory on success, negative number if not found
*/
int nitroromResolvePath(NitroRom* nr, u16 base_dir, const char* path);

MK_EXTERN_C_END

//! @}
