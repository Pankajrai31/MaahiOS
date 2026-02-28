/**
 * MaahiOS Filesystem Library - libfs.h
 * 
 * Description:
 *   User library for file and directory operations.
 *   Auto-initializes on first call (discovers FS Executive SHM, attaches).
 *   Filesystem-agnostic: works with ISO9660 now, MFS in the future.
 * 
 *   For block-level disk operations (raw sectors), use libdisk instead.
 * 
 * Usage:
 *   #include "libfs.h"
 *   
 *   fs_file_entry_t entries[32];
 *   int count = libfs_list_dir("/", entries, 32);
 *   
 *   uint8_t buf[4096];
 *   int bytes = libfs_read_file("/BOOT", "kernel.bin", buf, 4096);
 *   
 *   No init() needed — handled automatically.
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#ifndef LIBFS_H
#define LIBFS_H

#include <stdint.h>
#include "../../executives/fsexecutive/fs_executive.h"

/*=============================================================================
 * LIBRARY INITIALIZATION
 *===========================================================================*/

/**
 * libfs_init - Explicitly initialize filesystem library (optional)
 * 
 * Auto-called on first use of any libfs function.
 * Returns: 0 on success, negative if FS Executive not available
 */
int libfs_init(void);

/**
 * libfs_shutdown - Cleanup filesystem library
 * 
 * Detaches from SHM queues.
 */
void libfs_shutdown(void);

/*=============================================================================
 * DIRECTORY OPERATIONS
 *===========================================================================*/

/**
 * libfs_list_dir - List files in a directory
 * @path: Directory path (e.g., "/" or "/BOOT")
 * @entries: Output array of fs_file_entry_t
 * @max_entries: Maximum entries to return
 * 
 * Returns: Number of entries on success, negative on error
 */
int libfs_list_dir(const char *path, fs_file_entry_t *entries, int max_entries);

/**
 * libfs_file_count - Get number of files in a directory
 * @path: Directory path (e.g., "/")
 * 
 * Returns: File count on success, negative on error
 */
int libfs_file_count(const char *path);

/*=============================================================================
 * FILE OPERATIONS
 *===========================================================================*/

/**
 * libfs_read_file - Read a file's contents
 * @dir_path: Directory containing the file (e.g., "/" or "/BOOT")
 * @filename: Name of the file to read
 * @buffer: Output buffer
 * @max_size: Maximum bytes to read
 * 
 * Returns: Bytes read on success, negative on error
 */
int libfs_read_file(const char *dir_path, const char *filename,
                    void *buffer, uint32_t max_size);

#endif /* LIBFS_H */
