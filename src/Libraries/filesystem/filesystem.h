/**
 * MaahiOS - Filesystem API
 */

#ifndef MAAHI_FILESYSTEM_H
#define MAAHI_FILESYSTEM_H

/* File entry structure (matches kernel's iso_file_entry_t) */
typedef struct {
    char name[64];
    unsigned int lba;
    unsigned int size;
    int is_directory;
} MaahiFileEntry;

/**
 * Get ISO root directory info
 * @param lba       Output: root directory LBA
 * @param size      Output: root directory size
 */
void maahi_get_root_info(unsigned int *lba, unsigned int *size);

/**
 * List directory contents
 * @param lba       Directory LBA
 * @param size      Directory size
 * @param entries   Output array of file entries
 * @param max       Maximum entries to read
 * @return Number of entries read, or -1 on error
 */
int maahi_list_dir(unsigned int lba, unsigned int size, MaahiFileEntry *entries, int max);

#endif /* MAAHI_FILESYSTEM_H */
