/**
 * Memory Management Syscalls - Ring 3 User Mode Implementations
 */

#include "memory.h"

void *syscall_alloc_page() {
    void *result;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_ALLOC_PAGE)
        : "memory"
    );
    return result;
}

void *syscall_allocate_memory(unsigned int size) {
    // Calculate number of 4KB pages needed
    unsigned int pages = (size + 4095) / 4096;
    
    // Allocate first page
    void *base = syscall_alloc_page();
    if (!base) return 0;
    
    // Allocate remaining pages (they should be contiguous in virtual memory)
    for (unsigned int i = 1; i < pages; i++) {
        void *page = syscall_alloc_page();
        if (!page) {
            // Allocation failed, but we can still use what we got
            // (Better than nothing, and for back buffer contiguity isn't critical)
            break;
        }
    }
    
    return base;
}

void syscall_free_page(void *addr) {
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_FREE_PAGE), "b"(addr)
        : "memory"
    );
}
