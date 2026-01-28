#ifndef USER_ISO_FS_SYSCALLS_H
#define USER_ISO_FS_SYSCALLS_H

#include "../../syscall_numbers.h"

/**
 * ISO9660 Filesystem Syscalls - Ring 3 User Mode
 */

typedef struct {
    char     name[64];
    unsigned int size;
    unsigned int lba;
    unsigned char is_directory;
} iso_file_entry_t;

void syscall_iso_get_root(unsigned int *lba, unsigned int *size);
int syscall_iso_list_dir(unsigned int dir_lba, unsigned int dir_size, void *entries, int max_entries);
int syscall_iso_read_file(unsigned int file_lba, unsigned int file_size, void *buffer, unsigned int max_size);

#endif // USER_ISO_FS_SYSCALLS_H
