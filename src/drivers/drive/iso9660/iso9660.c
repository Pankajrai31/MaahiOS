/**
 * ISO 9660 Filesystem Driver
 * Reads directory entries from CD/DVD ISO images
 */

#include <stdint.h>
#include "../ata/ata.h"
#include "../../../managers/klog/klog.h"
#include "../../../system/libraries/shared/io.h"

// ISO 9660 Constants
#define ISO_SECTOR_SIZE 2048
#define ISO_VOLUME_DESCRIPTOR_START 16  // LBA 16 = first volume descriptor

// Volume Descriptor Types
#define ISO_VD_BOOT          0
#define ISO_VD_PRIMARY       1
#define ISO_VD_SUPPLEMENTARY 2
#define ISO_VD_PARTITION     3
#define ISO_VD_TERMINATOR    255

// Directory Entry Flags
#define ISO_FLAG_HIDDEN      (1 << 0)
#define ISO_FLAG_DIRECTORY   (1 << 1)
#define ISO_FLAG_ASSOCIATED  (1 << 2)

// ISO 9660 Primary Volume Descriptor (simplified)
typedef struct __attribute__((packed)) {
    uint8_t  type;                      // 0x00
    char     identifier[5];             // 0x01: "CD001"
    uint8_t  version;                   // 0x06
    uint8_t  unused1;                   // 0x07
    char     system_identifier[32];     // 0x08
    char     volume_identifier[32];     // 0x28
    uint8_t  unused2[8];                // 0x48
    uint32_t volume_space_size_le;      // 0x50 (Little Endian)
    uint32_t volume_space_size_be;      // 0x54 (Big Endian)
    uint8_t  unused3[32];               // 0x58
    uint16_t volume_set_size_le;        // 0x78
    uint16_t volume_set_size_be;        // 0x7A
    uint16_t volume_seq_number_le;      // 0x7C
    uint16_t volume_seq_number_be;      // 0x7E
    uint16_t logical_block_size_le;     // 0x80
    uint16_t logical_block_size_be;     // 0x82
    uint32_t path_table_size_le;        // 0x84
    uint32_t path_table_size_be;        // 0x88
    uint32_t path_table_lba_le;         // 0x8C
    uint32_t optional_path_table_lba_le;// 0x90
    uint32_t path_table_lba_be;         // 0x94
    uint32_t optional_path_table_lba_be;// 0x98
    uint8_t  root_directory_entry[34];  // 0x9C: Root directory record
    char     volume_set_identifier[128];// 0xBE
    // ... more fields follow
} iso_primary_volume_descriptor_t;

// ISO 9660 Directory Entry
typedef struct __attribute__((packed)) {
    uint8_t  length;                    // Length of this record
    uint8_t  ext_attr_length;           // Extended attribute length
    uint32_t extent_lba_le;             // LBA of file data (LE)
    uint32_t extent_lba_be;             // LBA of file data (BE)
    uint32_t data_length_le;            // File size (LE)
    uint32_t data_length_be;            // File size (BE)
    uint8_t  recording_date[7];         // Date/time
    uint8_t  flags;                     // File flags
    uint8_t  file_unit_size;            // File unit size
    uint8_t  interleave_gap;            // Interleave gap size
    uint16_t volume_seq_le;             // Volume sequence (LE)
    uint16_t volume_seq_be;             // Volume sequence (BE)
    uint8_t  name_length;               // Filename length
    char     name[1];                   // Variable length filename
} iso_directory_entry_t;

// File entry structure for user
typedef struct {
    char     name[64];
    uint32_t size;
    uint32_t lba;
    uint8_t  is_directory;
} iso_file_entry_t;

// Static buffer for sector reads
static uint8_t g_iso_sector_buffer[ISO_SECTOR_SIZE] __attribute__((aligned(4)));

// Cached root directory info
static uint32_t g_root_dir_lba = 0;
static uint32_t g_root_dir_size = 0;
static uint8_t  g_cdrom_drive_id = 0xFF;  // Invalid until initialized

/**
 * Read a 2048-byte sector from ATAPI drive using SCSI READ(12)
 */
int atapi_read_sector(uint8_t drive_id, uint32_t lba, void *buffer) {
    ata_drive_t *drive = ata_get_drive(drive_id);
    if (!drive || drive->type != ATA_DEVICE_TYPE_ATAPI) {
        return -1;
    }
    
    uint16_t base = drive->base_port;
    uint8_t slave = drive->is_slave;
    
    // Select drive
    outb(base + 6, slave ? 0xB0 : 0xA0);
    for (volatile int i = 0; i < 400; i++);  // 400ns delay
    
    // Set byte count (sector size)
    outb(base + 4, ISO_SECTOR_SIZE & 0xFF);
    outb(base + 5, (ISO_SECTOR_SIZE >> 8) & 0xFF);
    
    // Send PACKET command
    outb(base + 7, 0xA0);  // ATAPI PACKET command
    
    // Wait for DRQ
    int timeout = 100000;
    uint8_t status;
    while (timeout-- > 0) {
        status = inb(base + 7);
        if (status & 0x08) break;  // DRQ
        if (status & 0x01) return -2;  // Error
    }
    if (timeout <= 0) return -3;  // Timeout
    
    // Build SCSI READ(12) command
    uint8_t packet[12] = {
        0xA8,                           // READ(12) opcode
        0x00,                           // Flags
        (uint8_t)(lba >> 24),           // LBA byte 3
        (uint8_t)(lba >> 16),           // LBA byte 2
        (uint8_t)(lba >> 8),            // LBA byte 1
        (uint8_t)(lba),                 // LBA byte 0
        0x00, 0x00, 0x00, 0x01,         // Transfer length = 1 sector
        0x00, 0x00                      // Control
    };
    
    // Send packet (6 words)
    uint16_t *pkt16 = (uint16_t*)packet;
    for (int i = 0; i < 6; i++) {
        outw(base, pkt16[i]);
    }
    
    // Wait for data
    timeout = 100000;
    while (timeout-- > 0) {
        status = inb(base + 7);
        if (status & 0x08) break;  // DRQ - data ready
        if (status & 0x01) return -4;  // Error
        if (!(status & 0x80)) {
            // BSY clear, check if done
            if (!(status & 0x08)) {
                // No DRQ and no BSY - check for error
                if (status & 0x01) return -5;
                break;
            }
        }
    }
    if (timeout <= 0) return -6;  // Timeout waiting for data
    
    // Read sector data (1024 words = 2048 bytes)
    uint16_t *buf16 = (uint16_t*)buffer;
    for (int i = 0; i < 1024; i++) {
        buf16[i] = inw(base);
    }
    
    return 0;
}

/**
 * Initialize ISO9660 driver - find CDROM and read volume descriptor
 */
int iso9660_init(void) {
    KLOG_INFO("ISO9660", "Initializing ISO9660 filesystem driver");
    
    // Find first ATAPI drive
    for (int i = 0; i < 4; i++) {
        ata_drive_t *drive = ata_get_drive(i);
        if (drive && drive->type == ATA_DEVICE_TYPE_ATAPI) {
            g_cdrom_drive_id = i;
            KLOG_INFO("ISO9660", "Found ATAPI drive at index %d", i);
            break;
        }
    }
    
    if (g_cdrom_drive_id == 0xFF) {
        KLOG_ERROR("ISO9660", "No ATAPI drive found");
        return -1;
    }
    
    // Read Primary Volume Descriptor at LBA 16
    int ret = atapi_read_sector(g_cdrom_drive_id, ISO_VOLUME_DESCRIPTOR_START, g_iso_sector_buffer);
    if (ret != 0) {
        KLOG_ERROR("ISO9660", "Failed to read volume descriptor");
        return -2;
    }
    
    // Verify it's an ISO9660 volume
    iso_primary_volume_descriptor_t *pvd = (iso_primary_volume_descriptor_t*)g_iso_sector_buffer;
    
    if (pvd->identifier[0] != 'C' || pvd->identifier[1] != 'D' ||
        pvd->identifier[2] != '0' || pvd->identifier[3] != '0' ||
        pvd->identifier[4] != '1') {
        KLOG_ERROR("ISO9660", "Invalid volume identifier");
        return -3;
    }
    
    if (pvd->type != ISO_VD_PRIMARY) {
        KLOG_ERROR("ISO9660", "Not a primary volume descriptor");
        return -4;
    }
    
    // Extract root directory info from the embedded directory record
    iso_directory_entry_t *root = (iso_directory_entry_t*)pvd->root_directory_entry;
    g_root_dir_lba = root->extent_lba_le;
    g_root_dir_size = root->data_length_le;
    
    KLOG_INFO("ISO9660", "Root directory at LBA 0x%08X", g_root_dir_lba);
    
    KLOG_INFO("ISO9660", "Filesystem initialization complete");
    return 0;
}

/**
 * List files in root directory
 * Returns number of entries found, fills entries array
 */
int iso9660_list_root(iso_file_entry_t *entries, int max_entries) {
    if (g_cdrom_drive_id == 0xFF || g_root_dir_lba == 0) {
        return -1;  // Not initialized
    }
    
    int entry_count = 0;
    uint32_t bytes_remaining = g_root_dir_size;
    uint32_t current_lba = g_root_dir_lba;
    
    while (bytes_remaining > 0 && entry_count < max_entries) {
        // Read directory sector
        int ret = atapi_read_sector(g_cdrom_drive_id, current_lba, g_iso_sector_buffer);
        if (ret != 0) {
            break;
        }
        
        // Parse directory entries in this sector
        uint32_t offset = 0;
        while (offset < ISO_SECTOR_SIZE && entry_count < max_entries) {
            iso_directory_entry_t *de = (iso_directory_entry_t*)(g_iso_sector_buffer + offset);
            
            // End of entries?
            if (de->length == 0) {
                break;
            }
            
            // Skip "." and ".." entries (name_length == 1 with special chars)
            if (de->name_length == 1 && (de->name[0] == 0x00 || de->name[0] == 0x01)) {
                offset += de->length;
                continue;
            }
            
            // Copy entry info
            iso_file_entry_t *entry = &entries[entry_count];
            entry->lba = de->extent_lba_le;
            entry->size = de->data_length_le;
            entry->is_directory = (de->flags & ISO_FLAG_DIRECTORY) ? 1 : 0;
            
            // Copy filename (remove ";1" version suffix if present)
            int name_len = de->name_length;
            if (name_len > 63) name_len = 63;
            
            int j = 0;
            for (int i = 0; i < name_len && j < 63; i++) {
                if (de->name[i] == ';') break;  // Stop at version separator
                entry->name[j++] = de->name[i];
            }
            entry->name[j] = '\0';
            
            entry_count++;
            offset += de->length;
        }
        
        current_lba++;
        bytes_remaining = (bytes_remaining > ISO_SECTOR_SIZE) ? 
                          (bytes_remaining - ISO_SECTOR_SIZE) : 0;
    }
    
    return entry_count;
}

/**
 * Get file count in root directory
 */
int iso9660_get_file_count(void) {
    // Just count entries without storing them
    iso_file_entry_t dummy[32];
    return iso9660_list_root(dummy, 32);
}

/**
 * List files in a specific directory given its LBA and size
 */
int iso9660_list_directory(uint32_t dir_lba, uint32_t dir_size, iso_file_entry_t *entries, int max_entries) {
    if (g_cdrom_drive_id == 0xFF) {
        return -1;  // Not initialized
    }
    
    int entry_count = 0;
    uint32_t bytes_remaining = dir_size;
    uint32_t current_lba = dir_lba;
    
    while (bytes_remaining > 0 && entry_count < max_entries) {
        // Read directory sector
        int ret = atapi_read_sector(g_cdrom_drive_id, current_lba, g_iso_sector_buffer);
        if (ret != 0) {
            break;
        }
        
        // Parse directory entries in this sector
        uint32_t offset = 0;
        while (offset < ISO_SECTOR_SIZE && entry_count < max_entries) {
            iso_directory_entry_t *de = (iso_directory_entry_t*)(g_iso_sector_buffer + offset);
            
            // End of entries?
            if (de->length == 0) {
                break;
            }
            
            // Skip "." and ".." entries
            if (de->name_length == 1 && (de->name[0] == 0x00 || de->name[0] == 0x01)) {
                offset += de->length;
                continue;
            }
            
            // Copy entry info
            iso_file_entry_t *entry = &entries[entry_count];
            entry->lba = de->extent_lba_le;
            entry->size = de->data_length_le;
            entry->is_directory = (de->flags & ISO_FLAG_DIRECTORY) ? 1 : 0;
            
            // Copy filename (remove ";1" version suffix if present)
            int name_len = de->name_length;
            if (name_len > 63) name_len = 63;
            
            int j = 0;
            for (int i = 0; i < name_len && j < 63; i++) {
                if (de->name[i] == ';') break;
                entry->name[j++] = de->name[i];
            }
            entry->name[j] = '\0';
            
            entry_count++;
            offset += de->length;
        }
        
        current_lba++;
        bytes_remaining = (bytes_remaining > ISO_SECTOR_SIZE) ? 
                          (bytes_remaining - ISO_SECTOR_SIZE) : 0;
    }
    
    return entry_count;
}

/**
 * Find a directory entry by name in root and return its LBA/size
 * Returns 0 on success, -1 on error
 */
int iso9660_find_directory(const char *name, uint32_t *out_lba, uint32_t *out_size) {
    iso_file_entry_t entries[16];
    int count = iso9660_list_root(entries, 16);

    /* Compute effective name length: ignore trailing '/' or '\\' */
    int name_len = 0;
    while (name[name_len]) name_len++;
    while (name_len > 0 && (name[name_len - 1] == '/' || name[name_len - 1] == '\\'))
        name_len--;
    
    for (int i = 0; i < count; i++) {
        if (entries[i].is_directory) {
            // Simple case-insensitive compare up to name_len chars
            const char *a = entries[i].name;
            const char *b = name;
            int match = 1;
            int j = 0;
            while (*a && j < name_len) {
                char ca = *a >= 'a' && *a <= 'z' ? *a - 32 : *a;
                char cb = *b >= 'a' && *b <= 'z' ? *b - 32 : *b;
                if (ca != cb) {
                    match = 0;
                    break;
                }
                a++;
                b++;
                j++;
            }
            /* Both must end: entry name at '\0' and we consumed name_len chars */
            if (match && *a == '\0' && j == name_len) {
                *out_lba = entries[i].lba;
                *out_size = entries[i].size;
                return 0;
            }
        }
    }
    return -1;
}

/**
 * List files in /boot directory (convenience function)
 */
int iso9660_list_boot(iso_file_entry_t *entries, int max_entries) {
    uint32_t boot_lba, boot_size;
    
    // Find /boot directory
    if (iso9660_find_directory("BOOT", &boot_lba, &boot_size) != 0) {
        KLOG_WARN("ISO9660", "/boot directory not found");
        return -1;
    }
    
    KLOG_DEBUG("ISO9660", "Found /boot at LBA 0x%08X", boot_lba);
    
    return iso9660_list_directory(boot_lba, boot_size, entries, max_entries);
}

/**
 * Get root directory LBA
 */
uint32_t iso9660_get_root_lba(void) {
    return g_root_dir_lba;
}

/**
 * Get root directory size
 */
uint32_t iso9660_get_root_size(void) {
    return g_root_dir_size;
}

/**
 * Read file data from ISO given LBA and size
 * Reads the file into the provided buffer
 * 
 * @param file_lba LBA where file starts
 * @param file_size Size of file in bytes
 * @param buffer Output buffer (must be large enough!)
 * @param max_size Maximum bytes to read
 * @return Bytes read on success, negative on error
 */
int iso9660_read_file(uint32_t file_lba, uint32_t file_size, void *buffer, uint32_t max_size) {
    if (g_cdrom_drive_id == 0xFF) {
        KLOG_ERROR("ISO9660", "Not initialized");
        return -1;  // Not initialized
    }
    
    if (!buffer) {
        KLOG_ERROR("ISO9660", "NULL buffer");
        return -2;
    }
    
    // Limit read to max_size
    uint32_t bytes_to_read = (file_size < max_size) ? file_size : max_size;
    
    KLOG_DEBUG("ISO9660", "Reading file: LBA=0x%08X size=%u", file_lba, bytes_to_read);
    
    uint32_t bytes_read = 0;
    uint32_t current_lba = file_lba;
    uint8_t *dest = (uint8_t*)buffer;
    
    while (bytes_read < bytes_to_read) {
        // Read one sector
        int ret = atapi_read_sector(g_cdrom_drive_id, current_lba, g_iso_sector_buffer);
        if (ret != 0) {
            KLOG_ERROR("ISO9660", "Read sector failed at LBA 0x%08X", current_lba);
            return -3;
        }
        
        // How many bytes from this sector?
        uint32_t remaining = bytes_to_read - bytes_read;
        uint32_t to_copy = (remaining < ISO_SECTOR_SIZE) ? remaining : ISO_SECTOR_SIZE;
        
        // Copy to output buffer
        for (uint32_t i = 0; i < to_copy; i++) {
            dest[bytes_read + i] = g_iso_sector_buffer[i];
        }
        
        bytes_read += to_copy;
        current_lba++;
    }
    
    KLOG_DEBUG("ISO9660", "Read complete: %u bytes", bytes_read);
    
    return (int)bytes_read;
}

/**
 * Find a file by name in a directory and read its data
 * 
 * @param dir_lba Directory LBA to search in
 * @param dir_size Directory size
 * @param filename Name to search for (case-insensitive)
 * @param buffer Output buffer
 * @param max_size Maximum bytes to read
 * @return Bytes read on success, negative on error
 */
int iso9660_find_and_read_file(uint32_t dir_lba, uint32_t dir_size, 
                                const char *filename, void *buffer, uint32_t max_size) {
    // List directory to find the file
    iso_file_entry_t entries[32];
    int count = iso9660_list_directory(dir_lba, dir_size, entries, 32);
    
    if (count < 0) {
        return -1;
    }
    
    KLOG_DEBUG("ISO9660", "Searching for file: %s", filename);
    
    for (int i = 0; i < count; i++) {
        if (entries[i].is_directory) continue;
        
        // Case-insensitive compare
        const char *a = entries[i].name;
        const char *b = filename;
        int match = 1;
        while (*a && *b) {
            char ca = (*a >= 'a' && *a <= 'z') ? *a - 32 : *a;
            char cb = (*b >= 'a' && *b <= 'z') ? *b - 32 : *b;
            if (ca != cb) {
                match = 0;
                break;
            }
            a++;
            b++;
        }
        if (match && *a == *b) {
            // Found it!
            KLOG_DEBUG("ISO9660", "Found: %s", entries[i].name);
            return iso9660_read_file(entries[i].lba, entries[i].size, buffer, max_size);
        }
    }
    
    KLOG_WARN("ISO9660", "File not found: %s", filename);
    return -4;  // File not found
}
