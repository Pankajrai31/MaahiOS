/**
 * MaahiOS - Filesystem Implementation
 */

#include "filesystem.h"
#include "../core/syscall_helpers.h"

void maahi_get_root_info(unsigned int *lba, unsigned int *size) {
    register unsigned int *_lba __asm__("ebx") = lba;
    register unsigned int *_size __asm__("ecx") = size;
    
    /* Syscall 79: iso_get_root_info(lba_out, size_out) */
    __asm__ volatile(
        "mov $79, %%eax\n"
        "int $0x80\n"
        :
        : "b"(_lba), "c"(_size)
        : "memory"
    );
}

int maahi_list_dir(unsigned int lba, unsigned int size, MaahiFileEntry *entries, int max) {
    int result;
    register unsigned int _lba __asm__("ebx") = lba;
    register unsigned int _size __asm__("ecx") = size;
    register void *_buf __asm__("edx") = entries;
    register int _max __asm__("esi") = max;
    
    /* Syscall 78: iso_list_dir(lba, size, entries, max) */
    __asm__ volatile(
        "mov $78, %%eax\n"
        "int $0x80\n"
        : "=a"(result)
        : "b"(_lba), "c"(_size), "d"(_buf), "S"(_max)
        : "memory"
    );
    return result;
}
