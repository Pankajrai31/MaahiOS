/**
 * ISO9660 Filesystem Syscalls - Ring 3 User Mode Implementations
 */

#include "iso_fs.h"

void syscall_iso_get_root(unsigned int *lba, unsigned int *size) {
    asm volatile(
        "int $0x80"
        :
        : "a"(79), "b"(lba), "c"(size)
        : "memory"
    );
}

int syscall_iso_list_dir(unsigned int dir_lba, unsigned int dir_size, void *entries, int max_entries) {
    int result;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(78), "b"(dir_lba), "c"(dir_size), "d"(entries), "S"(max_entries)
        : "memory"
    );
    return result;
}

int syscall_iso_read_file(unsigned int file_lba, unsigned int file_size, void *buffer, unsigned int max_size) {
    int result;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(80), "b"(file_lba), "c"(file_size), "d"(buffer), "S"(max_size)
        : "memory"
    );
    return result;
}
