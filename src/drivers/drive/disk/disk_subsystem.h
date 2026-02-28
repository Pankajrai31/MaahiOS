#ifndef DISK_SUBSYSTEM_H
#define DISK_SUBSYSTEM_H

#include <stdint.h>

#define MAX_DISKS 8

// Disk types
#define DISK_TYPE_UNKNOWN   0
#define DISK_TYPE_HDD       1
#define DISK_TYPE_CDROM     2
#define DISK_TYPE_FLOPPY    3

// Filesystem types
#define FS_TYPE_UNKNOWN     0
#define FS_TYPE_ISO9660     1
#define FS_TYPE_FAT16       2
#define FS_TYPE_FAT32       3
#define FS_TYPE_NTFS        4

// Disk information structure
typedef struct {
    uint8_t active;           // 1 if disk exists
    uint8_t disk_type;        // DISK_TYPE_*
    uint8_t fs_type;          // FS_TYPE_*
    uint8_t drive_letter;     // 'A', 'B', 'C', etc.
    uint8_t ata_drive_id;     // ATA drive ID (0-3)
    uint32_t size_mb;         // Size in MB
    char label[32];           // Volume label
    char type_str[16];        // "Hard Disk", "CD-ROM", etc.
    char fs_str[16];          // "ISO 9660", "FAT32", etc.
} disk_info_t;

// Function prototypes

/**
 * Initialize disk subsystem.
 * @return 0 on success
 */
int disk_subsystem_init(void);
int disk_subsystem_scan(void);
disk_info_t* disk_subsystem_get_disk(uint8_t index);
int disk_subsystem_get_count(void);
int disk_subsystem_read_sector(uint8_t disk_index, uint32_t lba, void *buffer);

#endif // DISK_SUBSYSTEM_H
