/**
 * Memory Syscall Handlers
 * Domain: 32-47 (alloc_page, free_page, alloc_memory, atomic_memcpy)
 */

#include "../syscall_manager.h"
#include "../syscall_numbers.h"
#include "../../klog/klog.h"
#include "../../memory/pmm.h"
#include "../../memory/paging.h"
#include <stdint.h>

/* ===========================================================================
 * HANDLERS
 * =========================================================================== */

/**
 * sys_mem_alloc_page - Allocate a single 4KB page
 */
static int sys_mem_alloc_page(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                              uint32_t arg4, uint32_t arg5) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    
    void *page = vmm_alloc_page();
    
    if (!page) {
        KLOG_WARN("SYSCALL", "alloc_page: Out of memory");
        return 0;
    }
    
    KLOG_DEBUG_HEX("SYSCALL", "Allocated page at: ", (uint32_t)page);
    return (int)(uint32_t)page;
}

/**
 * sys_mem_free_page - Free a previously allocated page
 */
static int sys_mem_free_page(uint32_t addr, uint32_t arg2, uint32_t arg3,
                             uint32_t arg4, uint32_t arg5) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    
    if (addr == 0) {
        KLOG_WARN("SYSCALL", "free_page: NULL pointer");
        return SYSCALL_ERR_INVALID;
    }
    
    vmm_free_page((void *)addr);
    KLOG_DEBUG_HEX("SYSCALL", "Freed page at: ", addr);
    return 0;
}

/**
 * sys_mem_alloc - Allocate contiguous memory block
 */
static int sys_mem_alloc(uint32_t size_bytes, uint32_t arg2, uint32_t arg3,
                         uint32_t arg4, uint32_t arg5) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    
    if (size_bytes == 0) {
        KLOG_WARN("SYSCALL", "alloc_memory: Size is 0");
        return 0;
    }
    
    void *mem = vmm_alloc_size(size_bytes);
    
    if (!mem) {
        KLOG_WARN_HEX("SYSCALL", "alloc_memory: Failed for size: ", size_bytes);
        return 0;
    }
    
    KLOG_DEBUG_HEX2("SYSCALL", "Allocated memory addr/size: ", (uint32_t)mem, size_bytes);
    return (int)(uint32_t)mem;
}

/**
 * sys_mem_atomic_copy - Interrupt-safe memory copy
 */
static int sys_mem_atomic_copy(uint32_t dest, uint32_t src, uint32_t size,
                               uint32_t arg4, uint32_t arg5) {
    (void)arg4; (void)arg5;
    
    if (!dest || !src || size == 0) {
        return SYSCALL_ERR_INVALID;
    }
    
    /* Disable interrupts for atomic copy */
    __asm__ volatile("cli");
    
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    
    for (uint32_t i = 0; i < size; i++) {
        d[i] = s[i];
    }
    
    /* Re-enable interrupts */
    __asm__ volatile("sti");
    
    return 0;
}

/* ===========================================================================
 * REGISTRATION
 * =========================================================================== */

void syscall_register_memory_handlers(void) {
    syscall_register(SYS_MEM_ALLOC_PAGE, sys_mem_alloc_page);
    syscall_register(SYS_MEM_FREE_PAGE, sys_mem_free_page);
    syscall_register(SYS_MEM_ALLOC, sys_mem_alloc);
    syscall_register(SYS_MEM_ATOMIC_COPY, sys_mem_atomic_copy);
    
    KLOG_DEBUG("SYSCALL", "Memory handlers registered (32-47)");
}
