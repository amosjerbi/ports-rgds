/* Copied from calico (source/nds/nitrorom.c): the local libcalico_ds9.a lacks it. */
// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#include <stdlib.h>
#include <string.h>
#include <calico/types.h>
#include <calico/nds/nitrorom.h>

bool nitroromOpen(NitroRom* nr, const NitroRomParams* params, const NitroRomIface* iface, void* user)
{
	// Sanity check
	if (params->fat_sz < sizeof(NitroRomFile) || params->fat_sz > NITROROM_ROOT_DIR*sizeof(NitroRomFile) || params->fnt_sz < sizeof(NitroRomDir)) {
		return false;
	}

	memset(nr, 0, sizeof(*nr));
	nr->iface      = iface;
	nr->user       = user;
	nr->fnt_offset = params->fnt_offset;
	nr->img_offset = params->img_offset;

	NitroRomDir root_dir;
	if (!nitroromRead(nr, params->fnt_offset, &root_dir, sizeof(root_dir))) {
		nr->iface = NULL;
		return false;
	}

	nr->num_files  = params->fat_sz / sizeof(NitroRomFile);
	nr->num_dirs   = root_dir.num_dirs;
	nr->file_table = (NitroRomFile*)malloc(nr->num_files * sizeof(NitroRomFile));
	nr->dir_table  = (NitroRomDir*) malloc(nr->num_dirs  * sizeof(NitroRomDir));

	if (nr->dir_table) {
		nr->dir_table[0] = root_dir;
		nr->dir_table[0].parent_id = NITROROM_ROOT_DIR; // fixup to make it consistent with the other dirs

		if (nr->num_dirs > 1 && !nitroromRead(nr, params->fnt_offset + sizeof(NitroRomDir), &nr->dir_table[1], (nr->num_dirs-1) * sizeof(NitroRomDir))) {
			free(nr->dir_table);
			nr->dir_table = NULL;
		}
	}

	if (nr->file_table) {
		if (!nitroromRead(nr, params->fat_offset, nr->file_table, nr->num_files * sizeof(NitroRomFile))) {
			free(nr->file_table);
			nr->file_table = NULL;
		}
	}

	if (!nr->file_table || !nr->dir_table) {
		free(nr->file_table);
		free(nr->dir_table);
		nr->iface = NULL;
		return false;
	}

	return true;
}

void nitroromClose(NitroRom* nr)
{
	free(nr->file_table);
	free(nr->dir_table);
	nr->iface->close(nr->user);
	nr->iface = NULL;
}

bool nitroromReadIter(NitroRomIter* iter, NitroRomIterEntry* entry)
{
	struct {
		u8 name_len : 7;
		u8 is_dir   : 1;
	} u;

	if (!nitroromRead(iter->nr, iter->cursor, &u, 1)) {
		return false;
	}

	if (u.name_len == 0) {
		// End of directory
		return false;
	}

	unsigned read_len = u.name_len;
	if (u.is_dir) {
		read_len += sizeof(u16);
	}

	if (!nitroromRead(iter->nr, iter->cursor+1, entry, read_len)) {
		return false;
	}

	iter->cursor += 1 + read_len;

	if (u.is_dir) {
		entry->id = entry->name[read_len-2] | (entry->name[read_len-1] << 8);
	} else {
		entry->id = iter->cur_file_id++;
	}

	entry->name[u.name_len] = 0;
	entry->name_len = u.name_len;
	return true;
}

int nitroromResolvePath(NitroRom* nr, u16 base_dir, const char* path)
{
	if (!path || base_dir < NITROROM_ROOT_DIR || base_dir >= NITROROM_ROOT_DIR+nr->num_dirs) {
		return -1;
	}

	u16 cur_id = base_dir;
	const char* next_path = NULL;

	// Check for absolute paths
	if (path[0] == '/') {
		cur_id = NITROROM_ROOT_DIR;
	}

	for (;; path = next_path) {
		// Skip slashes
		while (*path == '/') path ++;

		// If no more path - we are done
		if (!*path) {
			return cur_id;
		}

		// Extract path component
		next_path = path;
		do { next_path ++; } while (*next_path && *next_path != '/');
		unsigned path_len = next_path - path;

		// Handle special paths
		if (path[0] == '.') {
			if (path_len == 1) {
				// Current dir (.)
				continue;
			} else if (path_len == 2 && path[1] == '.') {
				// Parent dir (..)
				cur_id = nitroromGetParentDir(nr, cur_id);
				continue;
			}
		}

		// Find the file or directory
		NitroRomIter it;
		NitroRomIterEntry ent;
		bool found = false;
		nitroromOpenIter(nr, cur_id, &it);
		while (nitroromReadIter(&it, &ent)) {
			if (ent.name_len == path_len && strncasecmp(ent.name, path, path_len) == 0) {
				found = true;
				cur_id = ent.id;
				break;
			}
		}

		// Fail if not found; or if we're expecting a dir and what we found was a file
		if (!found || (*next_path && cur_id < NITROROM_ROOT_DIR)) {
			return -1;
		}
	}
}
