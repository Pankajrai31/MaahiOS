/**
 * MaahiOS File System (MFS) - Layer 4
 * 
 * Implementation of the MFS filesystem driver.
 * Code-complete for future HDD support; not active until an MFS
 * partition is detected (CD-ROM volumes use ISO9660 via voldrive).
 * 
 * On-disk layout per partition:
 *   Cluster 0:     Superblock (1 sector used)
 *   Cluster 1-N:   MFT (Master File Table, MFS_MAX_ENTRIES * 128 bytes)
 *   Cluster N+1-M: Free bitmap (1 bit per cluster)
 *   Cluster M+1:   Data area start
 */

#include "mfs.h"
#include "../partition/partdrive.h"
#include "../../../managers/klog/klog.h"

/* ============================================
 * Internal helpers
 * ============================================ */

/* Sector buffer for MFS operations */
static uint8_t g_mfs_buffer[4096] __attribute__((aligned(4)));

/* How many sectors per cluster */
#define SECTORS_PER_CLUSTER (MFS_CLUSTER_SIZE / 512)

/* Convert cluster number to partition-relative LBA */
static uint32_t cluster_to_lba(uint32_t cluster) {
    return cluster * SECTORS_PER_CLUSTER;
}

/* Simple string length */
static int mfs_strlen(const char *s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

/* Simple string copy */
static void mfs_strcpy(char *dst, const char *src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

/* Case-insensitive string compare */
static int mfs_strcasecmp(const char *a, const char *b) {
    while (*a && *b) {
        char ca = (*a >= 'a' && *a <= 'z') ? *a - 32 : *a;
        char cb = (*b >= 'a' && *b <= 'z') ? *b - 32 : *b;
        if (ca != cb) return 0;
        a++; b++;
    }
    return (*a == *b);
}

/* Simple memset */
static void mfs_memset(void *dst, uint8_t val, uint32_t size) {
    uint8_t *d = (uint8_t *)dst;
    for (uint32_t i = 0; i < size; i++) d[i] = val;
}

/* Simple memcpy */
static void mfs_memcpy(void *dst, const void *src, uint32_t size) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (uint32_t i = 0; i < size; i++) d[i] = s[i];
}

/* Read a full cluster from partition */
static int read_cluster(uint8_t part_index, uint32_t cluster, void *buffer) {
    uint32_t lba = cluster_to_lba(cluster);
    uint32_t sector_size = partdrive_get_sector_size(part_index);
    uint32_t sectors = MFS_CLUSTER_SIZE / sector_size;
    return partdrive_read(part_index, lba, sectors, buffer);
}

/* Write a full cluster to partition */
static int write_cluster(uint8_t part_index, uint32_t cluster, const void *buffer) {
    uint32_t lba = cluster_to_lba(cluster);
    uint32_t sector_size = partdrive_get_sector_size(part_index);
    uint32_t sectors = MFS_CLUSTER_SIZE / sector_size;
    return partdrive_write(part_index, lba, sectors, buffer);
}

/* Read a single sector from partition */
static int read_sector(uint8_t part_index, uint32_t lba, void *buffer) {
    return partdrive_read(part_index, lba, 1, buffer);
}

/* Write a single sector to partition */
static int write_sector(uint8_t part_index, uint32_t lba, const void *buffer) {
    return partdrive_write(part_index, lba, 1, buffer);
}

/* ============================================
 * Bitmap operations
 * ============================================ */

/* Read the bitmap cluster(s) for a given data cluster and check/set bit */
static int bitmap_read_bit(mfs_context_t *ctx, uint32_t data_cluster) {
    uint32_t bit_offset = data_cluster;
    uint32_t byte_offset = bit_offset / 8;
    uint32_t cluster_in_bitmap = byte_offset / MFS_CLUSTER_SIZE;
    uint32_t offset_in_cluster = byte_offset % MFS_CLUSTER_SIZE;

    if (read_cluster(ctx->part_index,
                     ctx->sb.bitmap_start_cluster + cluster_in_bitmap,
                     g_mfs_buffer) != 0) {
        return -1;
    }

    return (g_mfs_buffer[offset_in_cluster] >> (bit_offset % 8)) & 1;
}

static int bitmap_set_bit(mfs_context_t *ctx, uint32_t data_cluster, int value) {
    uint32_t bit_offset = data_cluster;
    uint32_t byte_offset = bit_offset / 8;
    uint32_t cluster_in_bitmap = byte_offset / MFS_CLUSTER_SIZE;
    uint32_t offset_in_cluster = byte_offset % MFS_CLUSTER_SIZE;

    uint32_t bmp_cluster = ctx->sb.bitmap_start_cluster + cluster_in_bitmap;

    if (read_cluster(ctx->part_index, bmp_cluster, g_mfs_buffer) != 0) {
        return -1;
    }

    if (value) {
        g_mfs_buffer[offset_in_cluster] |= (1 << (bit_offset % 8));
    } else {
        g_mfs_buffer[offset_in_cluster] &= ~(1 << (bit_offset % 8));
    }

    return write_cluster(ctx->part_index, bmp_cluster, g_mfs_buffer);
}

/* Find a free cluster in the bitmap */
static int bitmap_alloc_cluster(mfs_context_t *ctx) {
    uint32_t total = ctx->sb.total_clusters - ctx->sb.data_start_cluster;

    for (uint32_t i = 0; i < total; i++) {
        int bit = bitmap_read_bit(ctx, i);
        if (bit == 0) {
            /* Found free cluster */
            if (bitmap_set_bit(ctx, i, 1) != 0) return -1;
            ctx->sb.free_clusters--;
            return (int)(ctx->sb.data_start_cluster + i);
        }
    }
    return -1;  /* No free clusters */
}

static void bitmap_free_cluster(mfs_context_t *ctx, uint32_t abs_cluster) {
    if (abs_cluster < ctx->sb.data_start_cluster) return;
    uint32_t rel = abs_cluster - ctx->sb.data_start_cluster;
    bitmap_set_bit(ctx, rel, 0);
    ctx->sb.free_clusters++;
}

/* ============================================
 * MFT operations
 * ============================================ */

/* Read an MFT entry by index */
static int mft_read_entry(mfs_context_t *ctx, uint16_t index, mfs_entry_t *entry) {
    if (index >= MFS_MAX_ENTRIES) return -1;

    /* Each entry is 128 bytes. Calculate which sector it's in */
    uint32_t entries_per_sector = 512 / sizeof(mfs_entry_t);  /* 4 entries per sector */
    uint32_t sector = index / entries_per_sector;
    uint32_t offset = (index % entries_per_sector) * sizeof(mfs_entry_t);

    uint32_t lba = cluster_to_lba(ctx->sb.mft_start_cluster) + sector;

    if (read_sector(ctx->part_index, lba, g_mfs_buffer) != 0) {
        return -1;
    }

    mfs_memcpy(entry, g_mfs_buffer + offset, sizeof(mfs_entry_t));
    return 0;
}

/* Write an MFT entry by index */
static int mft_write_entry(mfs_context_t *ctx, uint16_t index, const mfs_entry_t *entry) {
    if (index >= MFS_MAX_ENTRIES) return -1;

    uint32_t entries_per_sector = 512 / sizeof(mfs_entry_t);
    uint32_t sector = index / entries_per_sector;
    uint32_t offset = (index % entries_per_sector) * sizeof(mfs_entry_t);

    uint32_t lba = cluster_to_lba(ctx->sb.mft_start_cluster) + sector;

    /* Read-modify-write */
    if (read_sector(ctx->part_index, lba, g_mfs_buffer) != 0) {
        return -1;
    }

    mfs_memcpy(g_mfs_buffer + offset, entry, sizeof(mfs_entry_t));

    return write_sector(ctx->part_index, lba, g_mfs_buffer);
}

/* Find a free MFT entry (flags == MFS_FLAG_FREE) */
static int mft_alloc_entry(mfs_context_t *ctx) {
    mfs_entry_t entry;
    /* Start from 1 (0 = root) */
    for (uint16_t i = 1; i < MFS_MAX_ENTRIES; i++) {
        if (mft_read_entry(ctx, i, &entry) != 0) continue;
        if (entry.flags == MFS_FLAG_FREE) {
            return (int)i;
        }
    }
    return -1;  /* MFT full */
}

/* ============================================
 * Superblock operations
 * ============================================ */

static int superblock_write(mfs_context_t *ctx) {
    mfs_memset(g_mfs_buffer, 0, 512);
    mfs_memcpy(g_mfs_buffer, &ctx->sb, sizeof(mfs_superblock_t));
    return write_sector(ctx->part_index, 0, g_mfs_buffer);
}

/* ============================================
 * Public API implementation
 * ============================================ */

int mfs_format(uint8_t part_index, const char *label) {
    KLOG_INFO("MFS", "Formatting partition %d as MFS", part_index);

    partition_info_t *pinfo = partdrive_get_info(part_index);
    if (!pinfo || !pinfo->active) {
        KLOG_ERROR("MFS", "Invalid partition index %d", part_index);
        return -1;
    }

    /* Calculate layout */
    uint32_t sector_size = partdrive_get_sector_size(part_index);
    uint32_t total_sectors = pinfo->sector_count;
    uint32_t total_clusters = (total_sectors * sector_size) / MFS_CLUSTER_SIZE;

    if (total_clusters < 16) {
        KLOG_ERROR("MFS", "Partition too small for MFS (%d clusters)", total_clusters);
        return -2;
    }

    /* MFT: 256 entries * 128 bytes = 32KB = 8 clusters */
    uint32_t mft_clusters = (MFS_MAX_ENTRIES * sizeof(mfs_entry_t) + MFS_CLUSTER_SIZE - 1)
                            / MFS_CLUSTER_SIZE;

    /* Bitmap: 1 bit per cluster; total_clusters bits */
    uint32_t bitmap_bytes = (total_clusters + 7) / 8;
    uint32_t bitmap_clusters = (bitmap_bytes + MFS_CLUSTER_SIZE - 1) / MFS_CLUSTER_SIZE;

    uint32_t data_start = 1 + mft_clusters + bitmap_clusters;  /* cluster 0 = superblock */

    /* Build superblock */
    mfs_superblock_t sb;
    mfs_memset(&sb, 0, sizeof(sb));
    sb.magic              = MFS_MAGIC;
    sb.version            = MFS_VERSION;
    sb.total_clusters     = total_clusters;
    sb.cluster_size       = MFS_CLUSTER_SIZE;
    sb.mft_start_cluster  = 1;
    sb.mft_cluster_count  = mft_clusters;
    sb.bitmap_start_cluster = 1 + mft_clusters;
    sb.bitmap_cluster_count = bitmap_clusters;
    sb.data_start_cluster = data_start;
    sb.free_clusters      = total_clusters - data_start;
    sb.total_entries      = MFS_MAX_ENTRIES;
    if (label) {
        mfs_strcpy(sb.volume_label, label, sizeof(sb.volume_label));
    } else {
        mfs_strcpy(sb.volume_label, "MFS Volume", sizeof(sb.volume_label));
    }

    /* Write superblock */
    mfs_memset(g_mfs_buffer, 0, 512);
    mfs_memcpy(g_mfs_buffer, &sb, sizeof(sb));
    if (write_sector(part_index, 0, g_mfs_buffer) != 0) {
        KLOG_ERROR("MFS", "Failed to write superblock");
        return -3;
    }

    /* Zero out MFT area */
    mfs_memset(g_mfs_buffer, 0, MFS_CLUSTER_SIZE);
    for (uint32_t c = sb.mft_start_cluster;
         c < sb.mft_start_cluster + sb.mft_cluster_count; c++) {
        if (write_cluster(part_index, c, g_mfs_buffer) != 0) {
            KLOG_ERROR("MFS", "Failed to zero MFT cluster %d", c);
            return -4;
        }
    }

    /* Zero out bitmap area */
    for (uint32_t c = sb.bitmap_start_cluster;
         c < sb.bitmap_start_cluster + sb.bitmap_cluster_count; c++) {
        if (write_cluster(part_index, c, g_mfs_buffer) != 0) {
            KLOG_ERROR("MFS", "Failed to zero bitmap cluster %d", c);
            return -5;
        }
    }

    /* Create root directory (MFT entry 0) */
    mfs_entry_t root;
    mfs_memset(&root, 0, sizeof(root));
    root.flags        = MFS_FLAG_DIRECTORY;
    root.parent_entry = MFS_INVALID_ENTRY;  /* Root has no parent */
    root.first_cluster = 0;  /* No data cluster yet */
    root.size         = 0;
    mfs_strcpy(root.name, "/", sizeof(root.name));

    /* Build temporary context to write MFT entry */
    mfs_context_t tmp_ctx;
    tmp_ctx.mounted = 1;
    tmp_ctx.part_index = part_index;
    tmp_ctx.sb = sb;

    if (mft_write_entry(&tmp_ctx, MFS_ROOT_ENTRY, &root) != 0) {
        KLOG_ERROR("MFS", "Failed to write root directory entry");
        return -6;
    }

    KLOG_INFO("MFS", "Format complete: %d clusters, %d free, data@%d",
              total_clusters, sb.free_clusters, data_start);
    return 0;
}

int mfs_mount(mfs_context_t *ctx, uint8_t part_index) {
    if (!ctx) return -1;

    KLOG_INFO("MFS", "Mounting partition %d", part_index);

    ctx->mounted = 0;
    ctx->part_index = part_index;

    /* Read superblock (sector 0 of partition) */
    if (read_sector(part_index, 0, g_mfs_buffer) != 0) {
        KLOG_ERROR("MFS", "Failed to read superblock");
        return -2;
    }

    mfs_memcpy(&ctx->sb, g_mfs_buffer, sizeof(mfs_superblock_t));

    /* Validate */
    if (ctx->sb.magic != MFS_MAGIC) {
        KLOG_ERROR("MFS", "Invalid MFS magic: 0x%08X", ctx->sb.magic);
        return -3;
    }

    if (ctx->sb.version != MFS_VERSION) {
        KLOG_ERROR("MFS", "Unsupported MFS version: %d", ctx->sb.version);
        return -4;
    }

    ctx->mounted = 1;
    KLOG_INFO("MFS", "Mounted: '%s' (%d clusters, %d free)",
              ctx->sb.volume_label, ctx->sb.total_clusters, ctx->sb.free_clusters);
    return 0;
}

void mfs_unmount(mfs_context_t *ctx) {
    if (!ctx || !ctx->mounted) return;

    /* Flush superblock (free cluster count may have changed) */
    superblock_write(ctx);

    ctx->mounted = 0;
    KLOG_INFO("MFS", "Volume unmounted");
}

int mfs_list_dir(mfs_context_t *ctx, uint16_t dir_entry,
                 mfs_file_entry_t *entries, int max_entries) {
    if (!ctx || !ctx->mounted || !entries) return -1;

    int count = 0;
    mfs_entry_t mft_entry;

    /* Scan all MFT entries looking for children of dir_entry */
    for (uint16_t i = 0; i < MFS_MAX_ENTRIES && count < max_entries; i++) {
        if (mft_read_entry(ctx, i, &mft_entry) != 0) continue;
        if (mft_entry.flags == MFS_FLAG_FREE) continue;
        if (mft_entry.parent_entry != dir_entry) continue;
        if (i == dir_entry) continue;  /* Skip self */

        mfs_file_entry_t *out = &entries[count];
        mfs_strcpy(out->name, mft_entry.name, sizeof(out->name));
        out->size = mft_entry.size;
        out->mft_index = i;
        out->is_directory = (mft_entry.flags & MFS_FLAG_DIRECTORY) ? 1 : 0;

        count++;
    }

    return count;
}

int mfs_read_file(mfs_context_t *ctx, uint16_t mft_index,
                  void *buffer, uint32_t max_size) {
    if (!ctx || !ctx->mounted || !buffer) return -1;

    mfs_entry_t entry;
    if (mft_read_entry(ctx, mft_index, &entry) != 0) return -2;

    if (!(entry.flags & MFS_FLAG_FILE)) {
        KLOG_WARN("MFS", "Entry %d is not a file", mft_index);
        return -3;
    }

    uint32_t to_read = (entry.size < max_size) ? entry.size : max_size;
    if (to_read == 0) return 0;

    /* Read data from first_cluster (simple single-cluster for now) */
    /* TODO: Support multi-cluster files with cluster chain */
    uint32_t cluster = entry.first_cluster;
    if (cluster == 0) return 0;

    uint8_t *dst = (uint8_t *)buffer;
    uint32_t bytes_read = 0;

    /* Read cluster by cluster */
    while (bytes_read < to_read && cluster != 0) {
        if (read_cluster(ctx->part_index, cluster, g_mfs_buffer) != 0) {
            KLOG_ERROR("MFS", "Failed to read cluster %d", cluster);
            return -4;
        }

        uint32_t remaining = to_read - bytes_read;
        uint32_t copy_size = (remaining < MFS_CLUSTER_SIZE) ? remaining : MFS_CLUSTER_SIZE;
        mfs_memcpy(dst + bytes_read, g_mfs_buffer, copy_size);
        bytes_read += copy_size;

        /* Simple: consecutive clusters for now */
        cluster++;

        /* Check if next cluster is past data area */
        if (cluster >= ctx->sb.total_clusters) break;
    }

    return (int)bytes_read;
}

int mfs_write_file(mfs_context_t *ctx, uint16_t dir_entry,
                   const char *filename, const void *data, uint32_t size) {
    if (!ctx || !ctx->mounted || !filename) return -1;
    if (size > 0 && !data) return -1;

    /* Check if file already exists */
    int existing = mfs_find(ctx, dir_entry, filename);
    uint16_t mft_idx;

    if (existing >= 0) {
        /* Overwrite: free old clusters first */
        mfs_entry_t old_entry;
        if (mft_read_entry(ctx, (uint16_t)existing, &old_entry) == 0) {
            if (old_entry.first_cluster != 0) {
                /* Free old data clusters */
                uint32_t old_clusters = (old_entry.size + MFS_CLUSTER_SIZE - 1) / MFS_CLUSTER_SIZE;
                for (uint32_t i = 0; i < old_clusters; i++) {
                    bitmap_free_cluster(ctx, old_entry.first_cluster + i);
                }
            }
        }
        mft_idx = (uint16_t)existing;
    } else {
        /* Allocate new MFT entry */
        int idx = mft_alloc_entry(ctx);
        if (idx < 0) {
            KLOG_ERROR("MFS", "MFT full, cannot create file");
            return -2;
        }
        mft_idx = (uint16_t)idx;
    }

    /* Allocate data clusters */
    uint32_t clusters_needed = (size > 0) ? ((size + MFS_CLUSTER_SIZE - 1) / MFS_CLUSTER_SIZE) : 0;
    int first_cluster = 0;

    if (clusters_needed > 0) {
        first_cluster = bitmap_alloc_cluster(ctx);
        if (first_cluster < 0) {
            KLOG_ERROR("MFS", "No free clusters for file data");
            return -3;
        }

        /* Allocate consecutive clusters (simple approach) */
        for (uint32_t i = 1; i < clusters_needed; i++) {
            int c = bitmap_alloc_cluster(ctx);
            if (c < 0) {
                KLOG_ERROR("MFS", "Ran out of clusters at %d/%d", i, clusters_needed);
                /* Free already allocated */
                for (uint32_t j = 0; j < i; j++) {
                    bitmap_free_cluster(ctx, (uint32_t)first_cluster + j);
                }
                return -4;
            }
        }

        /* Write file data */
        const uint8_t *src = (const uint8_t *)data;
        uint32_t bytes_written = 0;
        uint32_t cluster = (uint32_t)first_cluster;

        for (uint32_t i = 0; i < clusters_needed; i++) {
            uint32_t remaining = size - bytes_written;
            uint32_t copy_size = (remaining < MFS_CLUSTER_SIZE) ? remaining : MFS_CLUSTER_SIZE;

            mfs_memset(g_mfs_buffer, 0, MFS_CLUSTER_SIZE);
            mfs_memcpy(g_mfs_buffer, src + bytes_written, copy_size);

            if (write_cluster(ctx->part_index, cluster + i, g_mfs_buffer) != 0) {
                KLOG_ERROR("MFS", "Failed to write data cluster %d", cluster + i);
                return -5;
            }

            bytes_written += copy_size;
        }
    }

    /* Write MFT entry */
    mfs_entry_t entry;
    mfs_memset(&entry, 0, sizeof(entry));
    entry.flags        = MFS_FLAG_FILE;
    entry.parent_entry = dir_entry;
    entry.first_cluster = (uint32_t)first_cluster;
    entry.size         = size;
    entry.create_time  = 0;  /* TODO: get time from time_manager */
    entry.modify_time  = 0;
    mfs_strcpy(entry.name, filename, MFS_MAX_FILENAME);

    if (mft_write_entry(ctx, mft_idx, &entry) != 0) {
        KLOG_ERROR("MFS", "Failed to write MFT entry %d", mft_idx);
        return -6;
    }

    /* Flush superblock (free count changed) */
    superblock_write(ctx);

    KLOG_INFO("MFS", "File '%s' written: %d bytes, MFT=%d, cluster=%d",
              filename, size, mft_idx, first_cluster);
    return (int)mft_idx;
}

int mfs_delete(mfs_context_t *ctx, uint16_t mft_index) {
    if (!ctx || !ctx->mounted) return -1;
    if (mft_index == MFS_ROOT_ENTRY) {
        KLOG_ERROR("MFS", "Cannot delete root directory");
        return -2;
    }

    mfs_entry_t entry;
    if (mft_read_entry(ctx, mft_index, &entry) != 0) return -3;
    if (entry.flags == MFS_FLAG_FREE) return -4;

    /* If directory, check it's empty */
    if (entry.flags & MFS_FLAG_DIRECTORY) {
        mfs_file_entry_t tmp[1];
        int children = mfs_list_dir(ctx, mft_index, tmp, 1);
        if (children > 0) {
            KLOG_WARN("MFS", "Directory not empty (%d entries)", children);
            return -5;
        }
    }

    /* Free data clusters */
    if (entry.first_cluster != 0 && entry.size > 0) {
        uint32_t clusters = (entry.size + MFS_CLUSTER_SIZE - 1) / MFS_CLUSTER_SIZE;
        for (uint32_t i = 0; i < clusters; i++) {
            bitmap_free_cluster(ctx, entry.first_cluster + i);
        }
    }

    /* Mark MFT entry as free */
    mfs_memset(&entry, 0, sizeof(entry));
    entry.flags = MFS_FLAG_FREE;
    mft_write_entry(ctx, mft_index, &entry);

    /* Flush superblock */
    superblock_write(ctx);

    return 0;
}

int mfs_create_dir(mfs_context_t *ctx, uint16_t parent_entry, const char *dirname) {
    if (!ctx || !ctx->mounted || !dirname) return -1;

    /* Check parent is a directory */
    mfs_entry_t parent;
    if (mft_read_entry(ctx, parent_entry, &parent) != 0) return -2;
    if (!(parent.flags & MFS_FLAG_DIRECTORY)) return -3;

    /* Check if name already exists */
    if (mfs_find(ctx, parent_entry, dirname) >= 0) {
        KLOG_WARN("MFS", "Directory '%s' already exists", dirname);
        return -4;
    }

    /* Allocate MFT entry */
    int idx = mft_alloc_entry(ctx);
    if (idx < 0) {
        KLOG_ERROR("MFS", "MFT full, cannot create directory");
        return -5;
    }

    /* Create directory entry */
    mfs_entry_t entry;
    mfs_memset(&entry, 0, sizeof(entry));
    entry.flags        = MFS_FLAG_DIRECTORY;
    entry.parent_entry = parent_entry;
    entry.first_cluster = 0;  /* Dirs don't need data clusters initially */
    entry.size         = 0;
    mfs_strcpy(entry.name, dirname, MFS_MAX_FILENAME);

    if (mft_write_entry(ctx, (uint16_t)idx, &entry) != 0) {
        KLOG_ERROR("MFS", "Failed to write dir MFT entry");
        return -6;
    }

    KLOG_INFO("MFS", "Directory '%s' created (MFT=%d, parent=%d)",
              dirname, idx, parent_entry);
    return idx;
}

int mfs_find(mfs_context_t *ctx, uint16_t dir_entry, const char *name) {
    if (!ctx || !ctx->mounted || !name) return -1;

    mfs_entry_t entry;
    for (uint16_t i = 0; i < MFS_MAX_ENTRIES; i++) {
        if (mft_read_entry(ctx, i, &entry) != 0) continue;
        if (entry.flags == MFS_FLAG_FREE) continue;
        if (entry.parent_entry != dir_entry) continue;
        if (i == dir_entry) continue;

        if (mfs_strcasecmp(entry.name, name)) {
            return (int)i;
        }
    }
    return -1;
}

int mfs_file_count(mfs_context_t *ctx, uint16_t dir_entry) {
    if (!ctx || !ctx->mounted) return -1;

    int count = 0;
    mfs_entry_t entry;
    for (uint16_t i = 0; i < MFS_MAX_ENTRIES; i++) {
        if (mft_read_entry(ctx, i, &entry) != 0) continue;
        if (entry.flags == MFS_FLAG_FREE) continue;
        if (entry.parent_entry != dir_entry) continue;
        if (i == dir_entry) continue;
        count++;
    }
    return count;
}
