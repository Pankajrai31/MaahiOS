/**
 * MaahiOS Partition Driver (Layer 3)
 * 
 * Manages disk partitions:
 *   - For CD-ROM: creates a virtual partition spanning the entire disc
 *   - For HDD: parses MBR at LBA 0 to discover up to 4 primary partitions
 *   - Translates partition-relative I/O into disk-absolute I/O
 */

#include "partdrive.h"
#include "../disk/disk.h"
#include "../../../managers/klog/klog.h"

/* ============================================
 * MBR structures
 * ============================================ */

/* MBR Partition Table Entry (16 bytes each, 4 entries at offset 446) */
typedef struct __attribute__((packed)) {
    uint8_t  boot_indicator;   /* 0x80 = bootable */
    uint8_t  start_chs[3];    /* CHS of first sector */
    uint8_t  type;             /* Partition type */
    uint8_t  end_chs[3];      /* CHS of last sector */
    uint32_t start_lba;        /* LBA of first sector */
    uint32_t sector_count;     /* Total sectors */
} mbr_partition_entry_t;

#define MBR_SIGNATURE_OFFSET  510
#define MBR_SIGNATURE         0xAA55
#define MBR_PARTITION_OFFSET  446

/* ============================================
 * Global partition registry
 * ============================================ */
static partition_info_t g_partitions[MAX_PARTITIONS] = {0};
static int g_partition_count = 0;

/* Temporary sector buffer for MBR reading */
static uint8_t g_mbr_buffer[512] __attribute__((aligned(4)));

/* ============================================
 * Internal helpers
 * ============================================ */

/**
 * Add a virtual partition for a whole-disc device (CD-ROM).
 * The entire disc is treated as one partition.
 */
static void partdrive_add_cdrom(uint8_t disk_index, disk_info_t *dinfo) {
    if (g_partition_count >= MAX_PARTITIONS) return;

    partition_info_t *p = &g_partitions[g_partition_count];
    p->active       = 1;
    p->disk_index   = disk_index;
    p->part_index   = 0;
    p->type         = PART_TYPE_ISO9660;
    p->bootable     = 0;
    p->start_lba    = 0;
    /* CD-ROM: estimate sector count from size_mb (2048-byte sectors) */
    p->sector_count = (dinfo->size_mb * 1024 * 1024) / 2048;
    p->size_mb      = dinfo->size_mb;

    KLOG_INFO("PART", "  Part[%d]: CD-ROM whole-disc (ISO9660) %dMB",
              g_partition_count, p->size_mb);

    g_partition_count++;
}

/**
 * Parse MBR on an HDD and add discovered partitions.
 */
static void partdrive_parse_mbr(uint8_t disk_index) {
    /* Read sector 0 (MBR) */
    if (disk_read_sectors(disk_index, 0, 1, g_mbr_buffer) != 0) {
        KLOG_WARN("PART", "  Failed to read MBR on disk %d", disk_index);
        return;
    }

    /* Check MBR signature */
    uint16_t sig = *(uint16_t *)(g_mbr_buffer + MBR_SIGNATURE_OFFSET);
    if (sig != MBR_SIGNATURE) {
        KLOG_WARN("PART", "  No valid MBR signature on disk %d (got 0x%04X)", disk_index, sig);
        return;
    }

    /* Parse 4 primary partition entries */
    mbr_partition_entry_t *entries = (mbr_partition_entry_t *)(g_mbr_buffer + MBR_PARTITION_OFFSET);

    for (int i = 0; i < 4; i++) {
        if (entries[i].type == PART_TYPE_EMPTY) continue;
        if (entries[i].sector_count == 0) continue;
        if (g_partition_count >= MAX_PARTITIONS) break;

        partition_info_t *p = &g_partitions[g_partition_count];
        p->active       = 1;
        p->disk_index   = disk_index;
        p->part_index   = (uint8_t)i;
        p->type         = entries[i].type;
        p->bootable     = entries[i].boot_indicator;
        p->start_lba    = entries[i].start_lba;
        p->sector_count = entries[i].sector_count;
        p->size_mb      = (uint32_t)((uint64_t)entries[i].sector_count * 512 / (1024 * 1024));

        KLOG_INFO("PART", "  Part[%d]: type=0x%02X start=%u count=%u (%dMB)",
                  g_partition_count, p->type, p->start_lba, p->sector_count, p->size_mb);

        g_partition_count++;
    }
}

/* ============================================
 * Public API
 * ============================================ */

int partdrive_init(void) {
    KLOG_INFO("PART", "Initializing partition driver");

    g_partition_count = 0;
    int disk_count = disk_get_count();

    for (uint8_t i = 0; i < (uint8_t)disk_count; i++) {
        disk_info_t *dinfo = disk_get_info(i);
        if (!dinfo || !dinfo->active) continue;

        KLOG_INFO("PART", "Scanning disk %d (%s)", i, dinfo->type_str);

        if (dinfo->disk_type == DISK_TYPE_CDROM) {
            partdrive_add_cdrom(i, dinfo);
        } else if (dinfo->disk_type == DISK_TYPE_HDD) {
            partdrive_parse_mbr(i);
        }
    }

    KLOG_INFO("PART", "Partition driver initialized (%d partitions)", g_partition_count);
    return 0;
}

int partdrive_get_count(void) {
    return g_partition_count;
}

partition_info_t *partdrive_get_info(uint8_t index) {
    if (index >= g_partition_count) return 0;
    if (!g_partitions[index].active) return 0;
    return &g_partitions[index];
}

int partdrive_read(uint8_t part_index, uint32_t offset_lba, uint32_t count, void *buffer) {
    if (part_index >= g_partition_count || !buffer) return -1;

    partition_info_t *p = &g_partitions[part_index];
    if (!p->active) return -1;

    /* For CD-ROM: offset_lba is already absolute (start_lba == 0) */
    /* For HDD: translate partition-relative to disk-absolute */
    uint32_t abs_lba = p->start_lba + offset_lba;

    return disk_read_sectors(p->disk_index, abs_lba, count, buffer);
}

int partdrive_write(uint8_t part_index, uint32_t offset_lba, uint32_t count, const void *buffer) {
    if (part_index >= g_partition_count || !buffer) return -1;

    partition_info_t *p = &g_partitions[part_index];
    if (!p->active) return -1;

    /* CD-ROM partitions are read-only */
    if (p->type == PART_TYPE_ISO9660) {
        KLOG_WARN("PART", "Cannot write to ISO9660 partition");
        return -3;
    }

    uint32_t abs_lba = p->start_lba + offset_lba;
    return disk_write_sectors(p->disk_index, abs_lba, count, buffer);
}

uint32_t partdrive_get_sector_size(uint8_t part_index) {
    if (part_index >= g_partition_count) return 0;
    return disk_get_sector_size(g_partitions[part_index].disk_index);
}

int partdrive_find_by_disk(uint8_t disk_index, uint8_t *out_parts, int max) {
    int found = 0;
    for (int i = 0; i < g_partition_count && found < max; i++) {
        if (g_partitions[i].active && g_partitions[i].disk_index == disk_index) {
            out_parts[found++] = (uint8_t)i;
        }
    }
    return found;
}

int partdrive_create_mbr(uint8_t disk_index, uint32_t total_sectors, uint8_t part_type) {
    if (total_sectors <= 1) {
        KLOG_ERROR("PART", "Disk %d too small for MBR (%u sectors)", disk_index, total_sectors);
        return -1;
    }

    /* Zero out the MBR buffer */
    for (int i = 0; i < 512; i++) {
        g_mbr_buffer[i] = 0;
    }

    /* Build partition entry 0: starts at LBA 1, spans rest of disk */
    mbr_partition_entry_t *entries = (mbr_partition_entry_t *)(g_mbr_buffer + MBR_PARTITION_OFFSET);

    uint32_t part_start = 1;  /* LBA 1 — MBR sits at LBA 0 */
    uint32_t part_sectors = total_sectors - 1;

    entries[0].boot_indicator = 0x00;       /* Not bootable — avoid BIOS boot from data disk */
    entries[0].type           = part_type;
    entries[0].start_lba      = part_start;
    entries[0].sector_count   = part_sectors;

    /* CHS values: not used in LBA mode, fill with standard values */
    entries[0].start_chs[0] = 0;
    entries[0].start_chs[1] = 1;  /* Head 0, Sector 1 */
    entries[0].start_chs[2] = 0;
    entries[0].end_chs[0]   = 0xFE;
    entries[0].end_chs[1]   = 0xFF;
    entries[0].end_chs[2]   = 0xFF;

    /* MBR signature */
    g_mbr_buffer[MBR_SIGNATURE_OFFSET]     = 0x55;
    g_mbr_buffer[MBR_SIGNATURE_OFFSET + 1] = 0xAA;

    /* Write MBR to LBA 0 */
    int ret = disk_write_sectors(disk_index, 0, 1, g_mbr_buffer);
    if (ret != 0) {
        KLOG_ERROR("PART", "Failed to write MBR to disk %d (err=%d)", disk_index, ret);
        return -2;
    }

    KLOG_INFO("PART", "MBR written to disk %d: type=0x%02X start=%u count=%u",
              disk_index, part_type, part_start, part_sectors);

    /* Re-parse to register the new partition in our table */
    /* First, remove any existing partitions from this disk */
    int new_count = 0;
    for (int i = 0; i < g_partition_count; i++) {
        if (g_partitions[i].disk_index != disk_index) {
            if (new_count != i) {
                g_partitions[new_count] = g_partitions[i];
            }
            new_count++;
        }
    }
    g_partition_count = new_count;

    /* Parse the newly written MBR */
    partdrive_parse_mbr(disk_index);

    /* Return the global partition index of the new partition */
    for (int i = 0; i < g_partition_count; i++) {
        if (g_partitions[i].disk_index == disk_index && g_partitions[i].active) {
            return i;
        }
    }

    return -3;  /* Should not happen */
}
