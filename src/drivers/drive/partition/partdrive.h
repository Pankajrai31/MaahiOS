/**
 * MaahiOS Partition Driver (Layer 3)
 * 
 * Manages disk partitions:
 *   - For CD-ROM: single partition = whole disk (ISO9660)
 *   - For HDD: parses MBR at LBA 0 to find partitions
 *   - Provides partition-relative read/write (translates to disk LBAs)
 * 
 * The volume driver (voldrive) uses partdrive to access filesystems.
 */

#ifndef PARTDRIVE_H
#define PARTDRIVE_H

#include <stdint.h>

#define MAX_PARTITIONS 16  /* Max partitions across all disks */

/* Partition type IDs (MBR type byte) */
#define PART_TYPE_EMPTY     0x00
#define PART_TYPE_FAT16     0x06
#define PART_TYPE_FAT32     0x0B
#define PART_TYPE_LINUX     0x83
#define PART_TYPE_MFS       0xAA  /* MaahiOS File System */
#define PART_TYPE_ISO9660   0xFF  /* Virtual: entire CD-ROM */

/* Partition information */
typedef struct {
    uint8_t  active;          /* 1 if valid partition */
    uint8_t  disk_index;      /* Which physical disk (index into disk manager) */
    uint8_t  part_index;      /* Partition index on that disk (0-3 for MBR) */
    uint8_t  type;            /* Partition type (PART_TYPE_*) */
    uint8_t  bootable;        /* 0x80 = bootable */
    uint32_t start_lba;       /* Start LBA on disk */
    uint32_t sector_count;    /* Number of sectors in partition */
    uint32_t size_mb;         /* Size in MB */
} partition_info_t;

/**
 * Initialize partition driver.
 * Scans all disks for partitions.
 * @return 0 on success
 */
int partdrive_init(void);

/**
 * Get total number of detected partitions.
 */
int partdrive_get_count(void);

/**
 * Get partition info by global partition index.
 * @param index Global partition index (0 to partdrive_get_count()-1)
 * @return Pointer to partition_info_t or NULL
 */
partition_info_t *partdrive_get_info(uint8_t index);

/**
 * Read sectors from a partition (partition-relative LBA).
 * Translates partition offset to disk-absolute LBA.
 * 
 * @param part_index  Partition index
 * @param offset_lba  LBA relative to partition start
 * @param count       Number of sectors
 * @param buffer      Output buffer
 * @return 0 on success, negative on error
 */
int partdrive_read(uint8_t part_index, uint32_t offset_lba, uint32_t count, void *buffer);

/**
 * Write sectors to a partition (partition-relative LBA).
 * 
 * @param part_index  Partition index
 * @param offset_lba  LBA relative to partition start
 * @param count       Number of sectors
 * @param buffer      Input buffer
 * @return 0 on success, negative on error
 */
int partdrive_write(uint8_t part_index, uint32_t offset_lba, uint32_t count, const void *buffer);

/**
 * Get the sector size for a partition (inherited from its disk).
 * @return Sector size in bytes
 */
uint32_t partdrive_get_sector_size(uint8_t part_index);

/**
 * Find partition index by disk index.
 * Useful for finding all partitions on a specific disk.
 * 
 * @param disk_index  Physical disk index
 * @param out_parts   Array to fill with partition indices
 * @param max         Maximum entries
 * @return Number of partitions found on that disk
 */
int partdrive_find_by_disk(uint8_t disk_index, uint8_t *out_parts, int max);

/**
 * Create an MBR on a disk with one partition spanning the whole disk.
 * Writes MBR to LBA 0, partition starts at LBA 1.
 * 
 * @param disk_index    Physical disk index
 * @param total_sectors Total sectors on the disk (from ATA IDENTIFY)
 * @param part_type     Partition type byte (e.g., PART_TYPE_MFS)
 * @return Global partition index of new partition, or negative on error
 */
int partdrive_create_mbr(uint8_t disk_index, uint32_t total_sectors, uint8_t part_type);

#endif /* PARTDRIVE_H */
