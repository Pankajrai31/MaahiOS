/**
 * MaahiOS Disk Manager (Layer 2)
 * 
 * Replaces disk_subsystem. Manages physical disks:
 *   - Calls ata_init() to detect drives
 *   - Provides disk_read_sectors()/disk_write_sectors() for partdrive
 *   - Registers with Device Manager for Disk Executive access
 *   - Maintains disk registry (disk_info_t array)
 */

#ifndef DISK_H
#define DISK_H

#include <stdint.h>

#define MAX_DISKS 8

/* Disk types */
#define DISK_TYPE_UNKNOWN   0
#define DISK_TYPE_HDD       1
#define DISK_TYPE_CDROM     2
#define DISK_TYPE_FLOPPY    3

/* Filesystem type hints (detected at scan time) */
#define FS_TYPE_UNKNOWN     0
#define FS_TYPE_ISO9660     1
#define FS_TYPE_MFS         2
#define FS_TYPE_FAT16       3
#define FS_TYPE_FAT32       4
#define FS_TYPE_NTFS        5

/* Disk information structure (same layout for Disk Executive compat) */
typedef struct {
    uint8_t  active;           /* 1 if disk exists */
    uint8_t  disk_type;        /* DISK_TYPE_* */
    uint8_t  fs_type;          /* FS_TYPE_* (hint from scan) */
    uint8_t  _reserved;        /* Was drive_letter; letters are volume-only now */
    uint8_t  ata_drive_id;     /* ATA drive ID (0-3) */
    uint32_t size_mb;          /* Size in MB */
    char     label[32];        /* Volume label */
    char     type_str[16];     /* "Hard Disk", "CD-ROM", etc. */
    char     fs_str[16];       /* "ISO 9660", "MFS", etc. */
} disk_info_t;

/**
 * Initialize disk manager.
 * Calls ata_init(), scans for drives, registers with Device Manager.
 * @return 0 on success
 */
int disk_init(void);

/**
 * Scan for all available ATA/ATAPI drives.
 * @return Number of drives found
 */
int disk_scan(void);

/**
 * Get disk info by index.
 * @param index Disk index (0 to disk_get_count()-1)
 * @return Pointer to disk_info_t or NULL
 */
disk_info_t *disk_get_info(uint8_t index);

/**
 * Get number of detected disks.
 * @return Disk count
 */
int disk_get_count(void);

/**
 * Read sectors from a physical disk.
 * For ATA: reads 512-byte sectors.
 * For ATAPI/CD-ROM: reads 2048-byte sectors.
 * 
 * @param disk_index  Disk index (0-based)
 * @param lba         Starting LBA
 * @param count       Number of sectors to read
 * @param buffer      Output buffer (must be large enough)
 * @return 0 on success, negative on error
 */
int disk_read_sectors(uint8_t disk_index, uint32_t lba, uint32_t count, void *buffer);

/**
 * Write sectors to a physical disk.
 * Only supported for ATA (HDD). ATAPI is read-only.
 * 
 * @param disk_index  Disk index (0-based)
 * @param lba         Starting LBA
 * @param count       Number of sectors to write
 * @param buffer      Input buffer
 * @return 0 on success, negative on error
 */
int disk_write_sectors(uint8_t disk_index, uint32_t lba, uint32_t count, const void *buffer);

/**
 * Get sector size for a disk.
 * @param disk_index Disk index
 * @return Sector size in bytes (512 for ATA, 2048 for ATAPI)
 */
uint32_t disk_get_sector_size(uint8_t disk_index);

#endif /* DISK_H */
