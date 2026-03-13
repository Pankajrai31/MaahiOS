/**
 * MaahiOS Disk Library - libdisk.h
 * 
 * Description:
 *   User library for block-level disk access.
 *   Auto-initializes on first call (discovers SHM, attaches).
 *   Falls back to direct SYS_DEV_* kernel syscalls if
 *   Disk Executive is not yet running.
 * 
 *   This library is for RAW DISK operations only:
 *   - List disks, get info, check status
 *   - Read/write raw sectors
 *   
 *   For filesystem operations (files, directories), use a future libfs.
 * 
 * Usage:
 *   #include "libdisk.h"
 *   
 *   int count = libdisk_list(disks, 8);
 *   libdisk_get_info(0, &info);
 *   int status = libdisk_get_status(0);
 *   int shm_id = libdisk_read_sector(0, lba, 1);
 *   No init() needed — handled automatically.
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
 * libdisk_init - Explicitly initialize disk library (optional)
 * 
 * Auto-called on first use of any libdisk function.
 * Falls back to direct device syscalls if executive not ready.
 * 
 * Returns: 0 on success, negative if executive not available
 */
int libdisk_init(void);

/**
 * libdisk_shutdown - Cleanup disk library
 * 
 * Detaches from SHM queues.
 */
void libdisk_shutdown(void);

/*=============================================================================
 * DISK ENUMERATION
 *===========================================================================*/

/**
 * libdisk_list - List available disks
 * @disks: Output array of disk_exec_info_t
 * @max_disks: Maximum entries to return (up to 5)
 * 
 * Returns: Number of disks found on success, negative on error
 */
int libdisk_list(disk_exec_info_t *disks, int max_disks);

/**
 * libdisk_get_count - Get number of disks
 * 
 * Returns: Disk count on success, negative on error
 */
int libdisk_get_count(void);

/*=============================================================================
 * DISK INFORMATION
 *===========================================================================*/

/**
 * libdisk_get_info - Get disk information
 * @disk_index: Disk index (0-based)
 * @info: Output structure
 * 
 * Returns: 0 on success, negative on error
 */
int libdisk_get_info(uint8_t disk_index, disk_exec_info_t *info);

/**
 * libdisk_get_status - Get disk online/offline status
 * @disk_index: Disk index (0-based)
 * 
 * Returns: DISK_STATUS_* value on success, negative on error
 */
int libdisk_get_status(uint8_t disk_index);

/**
 * libdisk_get_sector_size - Get sector size for a disk
 * @disk_index: Disk index (0-based)
 * 
 * Returns: Sector size in bytes (512 or 2048), negative on error
 */
int libdisk_get_sector_size(uint8_t disk_index);

/*=============================================================================
 * SECTOR OPERATIONS
 *===========================================================================*/

/**
 * libdisk_read_sector - Read a raw sector from disk
 * @disk_index: Disk index (0-based)
 * @lba: Logical Block Address
 * @count: Number of sectors to read (currently max 1)
 * 
 * Returns: SHM ID containing sector data on success.
 *          Caller must SHM_ATTACH to read data, then SHM_DETACH+SHM_DESTROY.
 *          Negative on error.
 */
int libdisk_read_sector(uint8_t disk_index, uint32_t lba, uint32_t count);

/*=============================================================================
 * DISK FORMAT
 *===========================================================================*/

/**
 * libdisk_format - Format a disk with MBR + MFS filesystem
 * @disk_index: Disk index (0-based, must be HDD)
 * @label: Volume label (max 31 chars, NULL for default "MaahiOS")
 * 
 * Creates one partition spanning the entire disk, formats it with MFS,
 * and mounts the new volume.
 * 
 * Returns: 0 on success, negative on error
 */
int libdisk_format(uint8_t disk_index, const char *label);

#endif /* LIBDISK_H */
