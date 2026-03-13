/**
 * MaahiOS File System (MFS) - Layer 4
 * 
 * A simple, hierarchical filesystem for MaahiOS:
 *   - On-disk layout: Superblock, MFT (Master File Table), Bitmap, Data clusters
 *   - Supports: files, directories (hierarchical), read, write, create, delete
 *   - Cluster-based allocation with free bitmap
 *   - Maximum: 256 MFT entries, 64-char filenames, 4GB volume
 * 
 * Code-complete but untestable without HDD image. All existing ISO9660
 * functionality continues to work through the voldrive routing layer.
 */

#ifndef MFS_H
#define MFS_H

#include <stdint.h>

/* ============================================
 * MFS On-Disk Constants
 * ============================================ */

#define MFS_MAGIC           0x4D465321  /* "MFS!" */
#define MFS_VERSION         1
#define MFS_CLUSTER_SIZE    4096        /* 4KB clusters (8 sectors of 512) */
#define MFS_MAX_FILENAME    60          /* Max filename length */
#define MFS_MAX_ENTRIES     256         /* Max MFT entries */
#define MFS_ROOT_ENTRY      0           /* MFT entry 0 = root directory */
#define MFS_INVALID_ENTRY   0xFFFF      /* Invalid MFT index */

/* MFT entry flags */
#define MFS_FLAG_FREE       0x00
#define MFS_FLAG_FILE       0x01
#define MFS_FLAG_DIRECTORY  0x02
#define MFS_FLAG_SYSTEM     0x04        /* System file (superblock, bitmap, etc.) */

/* ============================================
 * MFS On-Disk Structures
 * ============================================ */

/**
 * MFS Superblock - stored at cluster 0 (LBA 0 of partition)
 * Contains filesystem metadata.
 */
typedef struct __attribute__((packed)) {
    uint32_t magic;              /* MFS_MAGIC */
    uint32_t version;            /* MFS_VERSION */
    uint32_t total_clusters;     /* Total clusters on volume */
    uint32_t cluster_size;       /* Bytes per cluster (4096) */
    uint32_t mft_start_cluster;  /* First cluster of MFT area */
    uint32_t mft_cluster_count;  /* Clusters used by MFT */
    uint32_t bitmap_start_cluster; /* First cluster of free bitmap */
    uint32_t bitmap_cluster_count; /* Clusters used by bitmap */
    uint32_t data_start_cluster; /* First data cluster */
    uint32_t free_clusters;      /* Free cluster count */
    uint32_t total_entries;      /* Total MFT entries (MFS_MAX_ENTRIES) */
    char     volume_label[32];   /* Volume label */
    uint8_t  reserved[452];      /* Pad to exactly one sector (512 bytes) */
} mfs_superblock_t;

/**
 * MFS MFT Entry - describes one file or directory.
 * Each entry is 128 bytes; MFT holds up to MFS_MAX_ENTRIES.
 */
typedef struct __attribute__((packed)) {
    uint8_t  flags;              /* MFS_FLAG_* */
    uint8_t  reserved1;
    uint16_t parent_entry;       /* Parent directory MFT index */
    uint32_t first_cluster;      /* First data cluster */
    uint32_t size;               /* File size in bytes (0 for dirs) */
    uint32_t create_time;        /* Creation timestamp (Unix epoch) */
    uint32_t modify_time;        /* Last modification timestamp */
    char     name[MFS_MAX_FILENAME]; /* Null-terminated filename */
    uint8_t  reserved2[44];      /* Pad to 128 bytes */
} mfs_entry_t;

/* ============================================
 * In-Memory File Entry (for listing)
 * Compatible with iso_file_entry_t layout
 * ============================================ */
typedef struct {
    char     name[64];
    uint32_t size;
    uint32_t mft_index;          /* MFT entry index (instead of LBA) */
    uint8_t  is_directory;
} mfs_file_entry_t;

/* ============================================
 * MFS Volume Context (one per mounted MFS volume)
 * ============================================ */
typedef struct {
    uint8_t  mounted;            /* 1 if mounted */
    uint8_t  part_index;         /* Partition index (partdrive) */
    mfs_superblock_t sb;         /* Cached superblock */
} mfs_context_t;

/* ============================================
 * Public API
 * ============================================ */

/**
 * Format a partition with MFS.
 * Creates superblock, empty MFT, free bitmap, root directory.
 * 
 * @param part_index  Partition to format
 * @param label       Volume label (max 31 chars)
 * @return 0 on success, negative on error
 */
int mfs_format(uint8_t part_index, const char *label);

/**
 * Mount an MFS partition. Reads and validates superblock.
 * 
 * @param ctx         MFS context to initialize
 * @param part_index  Partition to mount
 * @return 0 on success, negative on error
 */
int mfs_mount(mfs_context_t *ctx, uint8_t part_index);

/**
 * Unmount an MFS volume.
 */
void mfs_unmount(mfs_context_t *ctx);

/**
 * List files in a directory.
 * 
 * @param ctx         Mounted MFS context
 * @param dir_entry   MFT index of directory (MFS_ROOT_ENTRY for root)
 * @param entries     Output array
 * @param max_entries Maximum entries to return
 * @return Count of entries, or negative on error
 */
int mfs_list_dir(mfs_context_t *ctx, uint16_t dir_entry,
                 mfs_file_entry_t *entries, int max_entries);

/**
 * Read a file's contents.
 * 
 * @param ctx        Mounted MFS context
 * @param mft_index  MFT entry of the file
 * @param buffer     Output buffer
 * @param max_size   Maximum bytes to read
 * @return Bytes read, or negative on error
 */
int mfs_read_file(mfs_context_t *ctx, uint16_t mft_index,
                  void *buffer, uint32_t max_size);

/**
 * Write (create or overwrite) a file.
 * 
 * @param ctx        Mounted MFS context
 * @param dir_entry  Parent directory MFT index
 * @param filename   Filename (max MFS_MAX_FILENAME chars)
 * @param data       File data
 * @param size       Data size in bytes
 * @return MFT index of file, or negative on error
 */
int mfs_write_file(mfs_context_t *ctx, uint16_t dir_entry,
                   const char *filename, const void *data, uint32_t size);

/**
 * Delete a file or empty directory.
 * 
 * @param ctx        Mounted MFS context
 * @param mft_index  MFT entry to delete
 * @return 0 on success, negative on error
 */
int mfs_delete(mfs_context_t *ctx, uint16_t mft_index);

/**
 * Create a subdirectory.
 * 
 * @param ctx        Mounted MFS context
 * @param parent_entry Parent directory MFT index
 * @param dirname    Directory name
 * @return MFT index of new directory, or negative on error
 */
int mfs_create_dir(mfs_context_t *ctx, uint16_t parent_entry, const char *dirname);

/**
 * Find a file/dir by name within a directory.
 * 
 * @param ctx        Mounted MFS context
 * @param dir_entry  Directory MFT index to search
 * @param name       Name to search for (case-insensitive)
 * @return MFT index if found, negative if not found
 */
int mfs_find(mfs_context_t *ctx, uint16_t dir_entry, const char *name);

/**
 * Get file count in a directory.
 */
int mfs_file_count(mfs_context_t *ctx, uint16_t dir_entry);

#endif /* MFS_H */
