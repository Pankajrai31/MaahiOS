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

/*=============================================================================
 * WRITE OPERATIONS (MFS only — ISO9660 is read-only)
 *===========================================================================*/

/**
 * libfs_write_file - Create or overwrite a file
 * @dir_path: Directory where the file goes (e.g., "/" or "/docs")
 * @filename: Name of the file (e.g., "readme.txt")
 * @data: Data buffer to write
 * @size: Size in bytes (0 to create empty file)
 * 
 * Returns: 0 on success, negative on error
 */
int libfs_write_file(const char *dir_path, const char *filename,
                     const void *data, uint32_t size);

/**
 * libfs_delete_file - Delete a file
 * @dir_path: Directory containing the file
 * @filename: File to delete
 * 
 * Returns: 0 on success, negative on error
 */
int libfs_delete_file(const char *dir_path, const char *filename);

/**
 * libfs_create_dir - Create a directory
 * @parent_path: Parent directory (e.g., "/")
 * @dirname: Name of new directory
 * 
 * Returns: 0 on success, negative on error
 */
int libfs_create_dir(const char *parent_path, const char *dirname);

/*=============================================================================
 * VOLUME QUERIES (no executive needed — direct syscalls)
 *===========================================================================*/

/**
 * Volume info returned by libfs_vol_info().
 * Must match kernel vol_info_user_t layout in fs_handlers.c.
 */
typedef struct {
    uint8_t  mounted;
    uint8_t  fs_type;
    uint8_t  part_index;
    char     drive_letter;
    uint32_t size_mb;
    char     label[32];
    char     fs_str[16];
} libfs_vol_info_t;

/**
 * libfs_vol_count - Get number of mounted volumes
 * Returns: volume count (>= 0), negative on error
 */
int libfs_vol_count(void);

/**
 * libfs_vol_info - Get info for a mounted volume
 * @vol_index: Volume index (0-based)
 * @info: Output structure (filled on success)
 * Returns: 0 on success, negative on error
 */
int libfs_vol_info(int vol_index, libfs_vol_info_t *info);

#endif /* LIBFS_H */
