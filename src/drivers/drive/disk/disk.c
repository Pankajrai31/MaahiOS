/**
 * MaahiOS Disk Manager (Layer 2)
 * 
 * Replaces disk_subsystem.c. Manages physical disks:
 *   - Initializes ATA subsystem
 *   - Scans for ATA/ATAPI drives
 *   - Provides sector read/write APIs for partdrive layer
 *   - Registers with Device Manager for Disk Executive compat
 */

#include "disk.h"
#include "../ata/ata.h"
#include "../../../managers/device/device_manager.h"
#include "../../../managers/cell/cell_manager.h"
#include "../../../managers/klog/klog.h"

/* ============================================
 * Global disk registry
 * ============================================ */
static disk_info_t g_disks[MAX_DISKS] = {0};
static int g_disk_count = 0;

/* ============================================
 * Forward declarations for Device Manager ops
 * ============================================ */
static int disk_dev_open(int flags);
static int disk_dev_close(int handle);
static int disk_dev_read(int handle, void *buffer, size_t size);
static int disk_dev_write(int handle, const void *buffer, size_t size);
static int disk_dev_ioctl(int handle, int cmd, void *arg);
static int disk_dev_poll(int handle);

/* Device operations table for Device Manager */
static device_ops_t disk_ops = {
    .open  = disk_dev_open,
    .close = disk_dev_close,
    .read  = disk_dev_read,
    .write = disk_dev_write,
    .ioctl = disk_dev_ioctl,
    .poll  = disk_dev_poll
};

/* ============================================
 * Helpers
 * ============================================ */

static void str_copy(char *dst, const char *src, int max_len) {
    int i = 0;
    while (src[i] && i < max_len - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

/* ============================================
 * ATAPI sector read (from iso9660.c, shared)
 * ============================================ */
extern int atapi_read_sector(uint8_t drive_id, uint32_t lba, void *buffer);

/* ============================================
 * Public API
 * ============================================ */

int disk_init(void) {
    KLOG_INFO("DISK", "Initializing disk manager");

    /* Initialize ATA subsystem (detects all 4 drive slots) */
    if (ata_init() != 0) {
        KLOG_ERROR("DISK", "ATA initialization failed");
        return -1;
    }

    /* Scan for drives */
    disk_scan();

    /* Register with Device Manager so Disk Executive can access us */
    register_device(DEV_DISK, "disk", &disk_ops);

    KLOG_INFO("DISK", "Disk manager initialized (%d drives)", g_disk_count);
    return 0;
}

int disk_scan(void) {
    KLOG_INFO("DISK", "Scanning for ATA/ATAPI drives");

    g_disk_count = 0;

    for (uint8_t i = 0; i < 4; i++) {
        ata_drive_t *ata_drive = ata_get_drive(i);
        if (ata_drive && ata_drive->exists) {
            if (g_disk_count < MAX_DISKS) {
                disk_info_t *disk = &g_disks[g_disk_count];

                disk->active = 1;
                disk->ata_drive_id = i;
                disk->_reserved = 0;

                if (ata_drive->type == ATA_DEVICE_TYPE_ATAPI) {
                    disk->disk_type = DISK_TYPE_CDROM;
                    str_copy(disk->type_str, "CD-ROM", sizeof(disk->type_str));
                    disk->fs_type = FS_TYPE_ISO9660;
                    str_copy(disk->fs_str, "ISO 9660", sizeof(disk->fs_str));
                    disk->size_mb = 700;
                } else if (ata_drive->type == ATA_DEVICE_TYPE_ATA) {
                    disk->disk_type = DISK_TYPE_HDD;
                    str_copy(disk->type_str, "Hard Disk", sizeof(disk->type_str));
                    disk->fs_type = FS_TYPE_UNKNOWN;
                    str_copy(disk->fs_str, "Unknown", sizeof(disk->fs_str));
                    disk->size_mb = ata_drive->size_mb;
                } else {
                    disk->disk_type = DISK_TYPE_UNKNOWN;
                    str_copy(disk->type_str, "Unknown", sizeof(disk->type_str));
                    disk->fs_type = FS_TYPE_UNKNOWN;
                    str_copy(disk->fs_str, "None", sizeof(disk->fs_str));
                    disk->size_mb = 0;
                }

                str_copy(disk->label, "MaahiOS_ISO", sizeof(disk->label));

                KLOG_INFO("DISK", "Found: %s (disk %d) ATA=%d",
                          disk->type_str, g_disk_count, i);

                g_disk_count++;
            }
        }
    }

    KLOG_INFO("DISK", "Scan complete: %d drive(s) found", g_disk_count);

    /* Publish disk info to cells so executives can read without ioctl */
    kernel_cell_write("device.disk.count", &g_disk_count, sizeof(int));

    for (int i = 0; i < g_disk_count; i++) {
        /* Build key: device.disk.N  (N = '0'..'7') */
        char key[32] = "device.disk.0";
        key[12] = '0' + (char)i;
        kernel_cell_write(key, &g_disks[i], sizeof(disk_info_t));

        KLOG_INFO("DISK", "Published disk %d to cell '%s'", i, key);
    }

    return g_disk_count;
}

disk_info_t *disk_get_info(uint8_t index) {
    if (index >= g_disk_count) return 0;
    if (!g_disks[index].active) return 0;
    return &g_disks[index];
}

int disk_get_count(void) {
    return g_disk_count;
}

uint32_t disk_get_sector_size(uint8_t disk_index) {
    if (disk_index >= g_disk_count) return 0;
    disk_info_t *d = &g_disks[disk_index];
    if (d->disk_type == DISK_TYPE_CDROM) return 2048;
    return 512;
}

int disk_read_sectors(uint8_t disk_index, uint32_t lba, uint32_t count, void *buffer) {
    if (disk_index >= g_disk_count || !buffer) return -1;
    disk_info_t *d = &g_disks[disk_index];
    if (!d->active) return -1;

    uint8_t ata_id = d->ata_drive_id;

    if (d->disk_type == DISK_TYPE_CDROM) {
        /* ATAPI: read 2048-byte sectors */
        uint8_t *dst = (uint8_t *)buffer;
        for (uint32_t i = 0; i < count; i++) {
            int ret = atapi_read_sector(ata_id, lba + i, dst + (i * 2048));
            if (ret != 0) {
                KLOG_ERROR("DISK", "ATAPI read failed: LBA=%u err=%d", lba + i, ret);
                return -2;
            }
        }
    } else {
        /* ATA: read 512-byte sectors */
        uint16_t *dst = (uint16_t *)buffer;
        for (uint32_t i = 0; i < count; i++) {
            int ret = ata_read_sector(ata_id, lba + i, dst + (i * 256));
            if (ret != 0) {
                KLOG_ERROR("DISK", "ATA read failed: LBA=%u err=%d", lba + i, ret);
                return -2;
            }
        }
    }

    return 0;
}

int disk_write_sectors(uint8_t disk_index, uint32_t lba, uint32_t count, const void *buffer) {
    if (disk_index >= g_disk_count || !buffer) return -1;
    disk_info_t *d = &g_disks[disk_index];
    if (!d->active) return -1;

    /* CD-ROM is read-only */
    if (d->disk_type == DISK_TYPE_CDROM) {
        KLOG_WARN("DISK", "Cannot write to CD-ROM (disk %d)", disk_index);
        return -3;
    }

    /* ATA: write 512-byte sectors */
    uint8_t ata_id = d->ata_drive_id;
    const uint16_t *src = (const uint16_t *)buffer;
    for (uint32_t i = 0; i < count; i++) {
        int ret = ata_write_sector(ata_id, lba + i, src + (i * 256));
        if (ret != 0) {
            KLOG_ERROR("DISK", "ATA write failed: LBA=%u err=%d", lba + i, ret);
            return -2;
        }
    }

    return 0;
}

/* ============================================
 * Device Manager Operations
 * (Same interface as old disk_subsystem for
 *  Disk Executive compatibility)
 * ============================================ */

static int disk_dev_open(int flags) {
    (void)flags;
    return 0;
}

static int disk_dev_close(int handle) {
    (void)handle;
    return 0;
}

static int disk_dev_read(int handle, void *buffer, size_t size) {
    (void)handle;
    if (!buffer || size < sizeof(disk_info_t)) return DEV_ERR_INVALID;
    if (g_disk_count > 0 && g_disks[0].active) {
        disk_info_t *info = (disk_info_t *)buffer;
        *info = g_disks[0];
        return sizeof(disk_info_t);
    }
    return 0;
}

static int disk_dev_write(int handle, const void *buffer, size_t size) {
    (void)handle; (void)buffer; (void)size;
    return DEV_ERR_NOT_SUPPORTED;
}

static int disk_dev_ioctl(int handle, int cmd, void *arg) {
    (void)handle;
    switch (cmd) {
        case DISK_IOCTL_GET_INFO: {
            if (!arg) return DEV_ERR_INVALID;
            uint8_t index = *(uint8_t *)arg;
            if (index >= g_disk_count) return DEV_ERR_NOT_FOUND;
            disk_info_t *disk = disk_get_info(index);
            if (!disk) return DEV_ERR_NOT_FOUND;
            disk_info_t *dest = (disk_info_t *)((uint8_t *)arg + 1);
            *dest = *disk;
            return DEV_OK;
        }
        case DISK_IOCTL_GET_SECTOR_SIZE:
            return 512;
        case DISK_IOCTL_GET_SECTOR_COUNT:
            return DEV_ERR_NOT_SUPPORTED;
        case DISK_IOCTL_FLUSH:
            return DEV_OK;
        default:
            return DEV_ERR_INVALID;
    }
}

static int disk_dev_poll(int handle) {
    (void)handle;
    return (g_disk_count > 0) ? 1 : 0;
}
