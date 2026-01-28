/**
 * Memory Management Syscalls
 * Handles memory allocation, deallocation, and memory operations
 */

#include "syscall_common.h"

/**
 * Handle memory-related syscalls
 * Returns: syscall return value, or -1 if syscall not handled
 */
unsigned int syscall_handle_memory(unsigned int syscall_num,
                                    unsigned int arg1,
                                    unsigned int arg2,
                                    unsigned int arg3,
                                    unsigned int arg4_esi,
                                    unsigned int arg5,
                                    unsigned int arg6) {
    unsigned int return_value = 0;
    
    switch(syscall_num) {
        case SYSCALL_ALLOC_PAGE:
            // Return address of allocated page
            return_value = (unsigned int)vmm_alloc_page();
            break;
            
        case SYSCALL_FREE_PAGE:
            // arg1 = address to free
            vmm_free_page((void*)arg1);
            break;
        
        case SYSCALL_ALLOC_MEMORY:
            // Allocate memory using vmm_alloc_size
            // arg1 = size in bytes
            return_value = (unsigned int)vmm_alloc_size(arg1);
            break;
        
        case SYSCALL_ATOMIC_MEMCPY: {
            // Interrupt-safe memory copy
            // arg1 = dest, arg2 = src, arg3 = size (in bytes)
            uint32_t *dest = (uint32_t*)arg1;
            uint32_t *src = (uint32_t*)arg2;
            uint32_t count = arg3 / 4;  // Convert bytes to dwords
            
            // Disable interrupts for atomic copy
            __asm__ volatile("cli");
            
            // Copy memory
            for (uint32_t i = 0; i < count; i++) {
                dest[i] = src[i];
            }
            
            // Re-enable interrupts
            __asm__ volatile("sti");
            
            return_value = 0;  // Success
            break;
        }
        
        default:
            return (unsigned int)-1;  // Not handled
    }
    
    return return_value;
}
