/**
 * MaahiOS Disk Library - libdisk.h
 * 
 * Description:
 *   User library for disk and file access.
 *   Applications include this header to read files and directories.
 *   Internally communicates with Disk Executive via SHM queues.
 * 
 * Usage:
 *   #include <libdisk.h>
 *   
 *   libdisk_init();
 *   char buffer[1024];
 *   int bytes = libdisk_read_file("/icons/app.bmp", 0, buffer, 1024);
 *   libdisk_shutdown();
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#ifndef LIBDISK_H
#define LIBDISK_H

#include <stdint.h>
#include "../../executives/diskexecutive/disk_executive.h"

/*=============================================================================
 * LIBRARY INITIALIZATION
 *===========================================================================*/

/**
 * libdisk_init - Initialize disk library
 * 
 * Connects to Disk Executive's SHM queues.
 * Must be called before any other libdisk functions.
 * 
 * Returns: 0 on success, negative on error
 */
int libdisk_init(void);

/**
 * libdisk_shutdown - Cleanup disk library
 */
void libdisk_shutdown(void);

/*=============================================================================
 * FILE OPERATIONS
 *===========================================================================*/

/**
 * libdisk_read_file - Read data from a file
 * @path: File path (e.g., "/icons/app.bmp")
 * @offset: Starting offset in file
 * @buffer: Buffer to receive data
 * @size: Maximum bytes to read
 * 
 * Returns: Bytes read on success, negative on error
 */
int libdisk_read_file(const char *path, uint32_t offset, void *buffer, uint32_t size);

/**
 * libdisk_file_exists - Check if a file exists
 * @path: File path
 * 
 * Returns: 1 if exists, 0 if not, negative on error
 */
int libdisk_file_exists(const char *path);

/**
 * libdisk_file_info - Get file information
 * @path: File path
 * @info: Output structure
 * 
 * Returns: 0 on success, negative on error
 */
int libdisk_file_info(const char *path, file_info_t *info);

/*=============================================================================
 * DIRECTORY OPERATIONS
 *===========================================================================*/

/**
 * libdisk_list_dir - List directory contents
 * @path: Directory path
 * @offset: Starting index
 * @entries: Output array of file_info_t
 * @max_entries: Maximum entries to return
 * @total_count: Output total number of entries (can be NULL)
 * 
 * Returns: Number of entries returned on success, negative on error
 */
int libdisk_list_dir(const char *path, uint32_t offset, file_info_t *entries, 
                     uint32_t max_entries, uint32_t *total_count);

/*=============================================================================
 * DISK OPERATIONS
 *===========================================================================*/

/**
 * libdisk_get_info - Get disk information
 * @disk_id: Disk ID (0 for primary)
 * @info: Output structure
 * 
 * Returns: 0 on success, negative on error
 */
int libdisk_get_info(uint32_t disk_id, disk_info_t *info);

/**
 * libdisk_list_disks - List available disks
 * @disks: Output array of disk_info_t
 * @max_disks: Maximum disks to return
 * 
 * Returns: Number of disks on success, negative on error
 */
int libdisk_list_disks(disk_info_t *disks, uint32_t max_disks);

#endif /* LIBDISK_H */
