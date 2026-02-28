#include "disk_subsystem.h"
#include "../ata/ata.h"
#include "../iso9660/iso9660.h"
#include "../../../managers/device/device_manager.h"
#include "../../../managers/klog/klog.h"

// Global disk registry
static disk_info_t g_disks[MAX_DISKS] = {0};
static int g_disk_count = 0;

/* ============================================
 * Device Manager Operations
 * ============================================ */

static int disk_dev_open(int flags) {
    (void)flags;
    return 0;  /* Always succeeds */
}

static int disk_dev_close(int handle) {
    (void)handle;
    return 0;
}

static int disk_dev_read(int handle, void *buffer, size_t size) {
    (void)handle;
    
    /* Read returns disk list info */
    if (!buffer || size < sizeof(disk_info_t)) {
        return DEV_ERR_INVALID;
    }
    
    /* Return first active disk info */
    if (g_disk_count > 0 && g_disks[0].active) {
        disk_info_t *info = (disk_info_t *)buffer;
        *info = g_disks[0];
        return sizeof(disk_info_t);
    }
    
    return 0;  /* No disks */
}

static int disk_dev_write(int handle, const void *buffer, size_t size) {
    (void)handle;
    (void)buffer;
    (void)size;
    /* Write not supported yet */
    return DEV_ERR_NOT_SUPPORTED;
}

static int disk_dev_ioctl(int handle, int cmd, void *arg) {
    (void)handle;
    
    switch (cmd) {
        case DISK_IOCTL_GET_INFO: {
            /* arg = pointer to struct { uint8_t index; disk_info_t *info; } */
            if (!arg) return DEV_ERR_INVALID;
            uint8_t index = *(uint8_t *)arg;
            if (index >= g_disk_count) return DEV_ERR_NOT_FOUND;
            disk_info_t *disk = disk_subsystem_get_disk(index);
            if (!disk) return DEV_ERR_NOT_FOUND;
            /* Copy info to arg+1 */
            disk_info_t *dest = (disk_info_t *)((uint8_t *)arg + 1);
            *dest = *disk;
            return DEV_OK;
        }
        
        case DISK_IOCTL_GET_SECTOR_SIZE:
            /* Return 512 for ATA, 2048 for ATAPI */
            return 512;
        
        case DISK_IOCTL_GET_SECTOR_COUNT:
            /* Not implemented */
            return DEV_ERR_NOT_SUPPORTED;
        
        case DISK_IOCTL_FLUSH:
            /* No cache to flush */
            return DEV_OK;
        
        default:
            return DEV_ERR_INVALID;
    }
}

static int disk_dev_poll(int handle) {
    (void)handle;
    return (g_disk_count > 0) ? 1 : 0;
}

/* Device operations table */
static device_ops_t disk_ops = {
    .open  = disk_dev_open,
    .close = disk_dev_close,
    .read  = disk_dev_read,
    .write = disk_dev_write,
    .ioctl = disk_dev_ioctl,
    .poll  = disk_dev_poll
};

// Simple string copy
static void strcpy_simple(char *dst, const char *src, int max_len) {
    int i = 0;
    while (src[i] && i < max_len - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

/**
 * Initialize disk subsystem
 * @return 0 on success
 */
int disk_subsystem_init(void) {
    KLOG_INFO("DISK", "Initializing disk subsystem");
    
    // Initialize ATA subsystem
    ata_init();
    
    // Scan for disks
    disk_subsystem_scan();
    
    // Initialize ISO9660 filesystem driver
    iso9660_init();
    
    // Register with Device Manager
    register_device(DEV_DISK, "disk", &disk_ops);
    
    KLOG_INFO("DISK", "Disk subsystem initialized and registered");
    
    return 0;  /* Success */
}

/**
 * Scan for all available disks
 */
int disk_subsystem_scan(void) {
    KLOG_INFO("DISK", "Scanning for ATA/ATAPI drives");
    
    g_disk_count = 0;
    char drive_letter = 'C';  // Start from C: (A: and B: reserved for floppies)
    
    // Scan all 4 ATA drives
    for (uint8_t i = 0; i < 4; i++) {
        ata_drive_t *ata_drive = ata_get_drive(i);
        if (ata_drive && ata_drive->exists) {
            if (g_disk_count < MAX_DISKS) {
                disk_info_t *disk = &g_disks[g_disk_count];
                
                disk->active = 1;
                disk->ata_drive_id = i;
                disk->drive_letter = drive_letter++;
                
                // Set disk type based on ATA type
                if (ata_drive->type == ATA_DEVICE_TYPE_ATAPI) {
                    disk->disk_type = DISK_TYPE_CDROM;
                    strcpy_simple(disk->type_str, "CD-ROM", sizeof(disk->type_str));
                    disk->fs_type = FS_TYPE_ISO9660;
                    strcpy_simple(disk->fs_str, "ISO 9660", sizeof(disk->fs_str));
                    disk->size_mb = 700;  // Typical CD size
                } else if (ata_drive->type == ATA_DEVICE_TYPE_ATA) {
                    disk->disk_type = DISK_TYPE_HDD;
                    strcpy_simple(disk->type_str, "Hard Disk", sizeof(disk->type_str));
                    disk->fs_type = FS_TYPE_UNKNOWN;
                    strcpy_simple(disk->fs_str, "Unknown", sizeof(disk->fs_str));
                    disk->size_mb = ata_drive->size_mb;
                } else {
                    disk->disk_type = DISK_TYPE_UNKNOWN;
                    strcpy_simple(disk->type_str, "Unknown", sizeof(disk->type_str));
                    disk->fs_type = FS_TYPE_UNKNOWN;
                    strcpy_simple(disk->fs_str, "None", sizeof(disk->fs_str));
                    disk->size_mb = 0;
                }
                
                strcpy_simple(disk->label, "MaahiOS_ISO", sizeof(disk->label));
                
                KLOG_INFO("DISK", "Found: %s (%c:)", disk->type_str, disk->drive_letter);
                
                g_disk_count++;
            }
        }
    }
    
    KLOG_INFO("DISK", "Scan complete: %d drive(s) found", g_disk_count);
    
    return g_disk_count;
}

/**
 * Get disk information by index
 */
disk_info_t* disk_subsystem_get_disk(uint8_t index) {
    if (index >= g_disk_count) return 0;
    if (!g_disks[index].active) return 0;
    return &g_disks[index];
}

/**
 * Get total number of disks
 */
int disk_subsystem_get_count(void) {
    return g_disk_count;
}

/**
 * Read a sector from disk
 */
int disk_subsystem_read_sector(uint8_t disk_index, uint32_t lba, void *buffer) {
    if (disk_index >= g_disk_count) return -1;
    
    disk_info_t *disk = &g_disks[disk_index];
    if (!disk->active) return -2;
    
    // Use ATA driver to read sector
    return ata_read_sector(disk->ata_drive_id, lba, (uint16_t*)buffer);
}
