/**
 * MaahiOS Volume Driver (Layer 5)
 * 
 * Top of the storage stack. Manages mounted volumes:
 *   - Assigns drive letters (C:, D:, etc.)
 *   - Auto-mounts partitions at boot (ISO9660 or MFS)
 *   - Routes filesystem operations to the correct FS driver
 *   - Provides unified API that fs_handlers.c calls
 * 
 * The fs_handlers.c syscall handlers call voldrive functions instead
 * of calling iso9660 directly. This allows transparent support for
 * both ISO9660 (read-only CD) and MFS (read-write HDD) volumes.
 */

#ifndef VOLDRIVE_H
#define VOLDRIVE_H

#include <stdint.h>
#include "../mfs/mfs.h"

#define MAX_VOLUMES 8

/* Volume filesystem type */
#define VOL_FS_NONE     0
#define VOL_FS_ISO9660  1
#define VOL_FS_MFS      2

/* Generic file entry (compatible with iso_file_entry_t) */
typedef struct {
    char     name[64];
    uint32_t size;
    uint32_t lba;            /* LBA for ISO9660, MFT index for MFS */
    uint8_t  is_directory;
} vol_file_entry_t;

/* Volume information */
typedef struct {
    uint8_t  mounted;        /* 1 if mounted */
    uint8_t  fs_type;        /* VOL_FS_* */
    uint8_t  part_index;     /* Partition index (partdrive) */
    char     drive_letter;   /* 'C', 'D', etc. */
    char     label[32];      /* Volume label */

    /* FS-specific context */
    mfs_context_t mfs_ctx;   /* Only valid if fs_type == VOL_FS_MFS */
} volume_t;

/**
 * Initialize volume driver.
 * Auto-mounts all discovered partitions.
 * @return 0 on success
 */
int voldrive_init(void);

/**
 * Get number of mounted volumes.
 */
int voldrive_get_count(void);

/**
 * Get volume info by index.
 */
volume_t *voldrive_get_volume(uint8_t index);

/**
 * Find volume by drive letter.
 * @param letter Drive letter ('C', 'D', etc.)
 * @return Volume index, or -1 if not found
 */
int voldrive_find_by_letter(char letter);

/* ============================================
 * Unified FS operations (used by fs_handlers.c)
 * These route to the correct FS driver based on
 * the volume's filesystem type.
 * ============================================ */

/**
 * List directory contents on a volume.
 * 
 * @param vol_index  Volume index
 * @param path       Path string ("/" for root, "/BOOT" for subdir)
 * @param entries    Output array (vol_file_entry_t, same layout as iso_file_entry_t)
 * @param max        Maximum entries
 * @return Count of entries, or negative on error
 */
int voldrive_list_dir(uint8_t vol_index, const char *path,
                      vol_file_entry_t *entries, int max);

/**
 * Read a file from a volume.
 * 
 * @param vol_index   Volume index
 * @param dir_path    Directory path
 * @param filename    Filename to read
 * @param buffer      Output buffer
 * @param max_size    Maximum bytes to read
 * @return Bytes read, or negative on error
 */
int voldrive_read_file(uint8_t vol_index, const char *dir_path,
                       const char *filename, void *buffer, uint32_t max_size);

/**
 * Get file count in a directory.
 */
int voldrive_file_count(uint8_t vol_index, const char *path);

/**
 * Find a subdirectory.
 * 
 * @param vol_index  Volume index
 * @param name       Directory name to find
 * @param out_lba    Output: LBA (ISO9660) or MFT index (MFS)
 * @param out_size   Output: size in bytes
 * @return 0 on success, negative on error
 */
int voldrive_find_dir(uint8_t vol_index, const char *name,
                      uint32_t *out_lba, uint32_t *out_size);

/**
 * Get root directory info.
 * @return 0 on success
 */
int voldrive_get_root_info(uint8_t vol_index, uint32_t *out_lba, uint32_t *out_size);

/**
 * Write a file to a volume (MFS only, ISO9660 is read-only).
 * 
 * @param vol_index  Volume index
 * @param dir_path   Directory path
 * @param filename   Filename
 * @param data       File data
 * @param size       Data size
 * @return 0 on success, negative on error
 */
int voldrive_write_file(uint8_t vol_index, const char *dir_path,
                        const char *filename, const void *data, uint32_t size);

/**
 * Delete a file from a volume (MFS only).
 */
int voldrive_delete_file(uint8_t vol_index, const char *dir_path,
                         const char *filename);

/**
 * Create a directory on a volume (MFS only).
 */
int voldrive_create_dir(uint8_t vol_index, const char *parent_path,
                        const char *dirname);

/**
 * Get the default volume index (first mounted volume, typically C:).
 * For backward compatibility with code that doesn't specify a volume.
 * @return Volume index, or -1 if nothing mounted
 */
int voldrive_get_default(void);

/**
 * Format a disk: create MBR partition → MFS filesystem → mount volume.
 * The entire disk becomes one MFS partition.
 * 
 * @param disk_index  Physical disk index (must be HDD, not CD-ROM)
 * @param label       Volume label (max 31 chars)
 * @return 0 on success, negative on error
 *   -1: invalid disk
 *   -2: disk is CD-ROM (read-only)
 *   -3: MBR creation failed
 *   -4: MFS format failed
 *   -5: volume mount failed
 */
int voldrive_format_disk(uint8_t disk_index, const char *label);

#endif /* VOLDRIVE_H */
