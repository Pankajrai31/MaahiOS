/**
 * ISO 9660 Filesystem Driver Header
 */

#ifndef ISO9660_H
#define ISO9660_H

#include <stdint.h>

// File entry structure for user
typedef struct {
    char     name[64];
    uint32_t size;
    uint32_t lba;
    uint8_t  is_directory;
} iso_file_entry_t;

// Initialize ISO9660 driver
int iso9660_init(void);

// List files in root directory (returns count, fills entries array)
int iso9660_list_root(iso_file_entry_t *entries, int max_entries);

// List files in /boot directory (convenience function)
int iso9660_list_boot(iso_file_entry_t *entries, int max_entries);

// List files in a specific directory by LBA
int iso9660_list_directory(uint32_t dir_lba, uint32_t dir_size, iso_file_entry_t *entries, int max_entries);

// Find a directory by name, returns LBA and size
int iso9660_find_directory(const char *name, uint32_t *out_lba, uint32_t *out_size);

// Get file count in root directory
int iso9660_get_file_count(void);

// Get root directory LBA
uint32_t iso9660_get_root_lba(void);

// Get root directory size  
uint32_t iso9660_get_root_size(void);

// Read a sector from ATAPI drive
int atapi_read_sector(uint8_t drive_id, uint32_t lba, void *buffer);

// Read file data from ISO given LBA and size
int iso9660_read_file(uint32_t file_lba, uint32_t file_size, void *buffer, uint32_t max_size);

// Find a file by name in a directory and read its data
int iso9660_find_and_read_file(uint32_t dir_lba, uint32_t dir_size, 
                                const char *filename, void *buffer, uint32_t max_size);

#endif // ISO9660_H
