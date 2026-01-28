#include "disk_subsystem.h"
#include "ata.h"
#include "iso9660.h"

// External kernel functions
extern void serial_print(const char *str);

// Global disk registry
static disk_info_t g_disks[MAX_DISKS] = {0};
static int g_disk_count = 0;

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
 */
void disk_subsystem_init(void) {
    serial_print("[DISK_SUBSYS] Initializing disk subsystem...\n");
    
    // Initialize ATA subsystem
    ata_init();
    
    // Scan for disks
    disk_subsystem_scan();
    
    // Initialize ISO9660 filesystem driver
    iso9660_init();
    
    serial_print("[DISK_SUBSYS] Initialization complete\n");
}

/**
 * Scan for all available disks
 */
int disk_subsystem_scan(void) {
    serial_print("[DISK_SUBSYS] Scanning for disks...\n");
    
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
                
                serial_print("[DISK_SUBSYS] Found disk: ");
                serial_print(disk->type_str);
                serial_print(" (");
                char letter[2] = {disk->drive_letter, '\0'};
                serial_print(letter);
                serial_print(":)\n");
                
                g_disk_count++;
            }
        }
    }
    
    serial_print("[DISK_SUBSYS] Scan complete. Found ");
    char count_str[4];
    count_str[0] = '0' + g_disk_count;
    count_str[1] = '\0';
    serial_print(count_str);
    serial_print(" disks\n");
    
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
