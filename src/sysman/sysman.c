#include "../syscalls/user_syscalls.h"
#include "../lib/heap.h"

/* Helper to print hex from Ring 3 */
static void print_hex_user(unsigned int val) {
    char hex[11];
    hex[0] = '0';
    hex[1] = 'x';
    for (int i = 9; i >= 2; i--) {
        unsigned int digit = val & 0xF;
        hex[i] = (digit < 10) ? ('0' + digit) : ('A' + digit - 10);
        val >>= 4;
    }
    hex[10] = '\0';
    syscall_puts(hex);
}

/* Ring 3 system manager main function */
void sysman_main_c(void) {
    syscall_puts("\n================================\n");
    syscall_puts("   MaahiOS System Manager\n");
    syscall_puts("   Ring 3 + Paging Active\n");
    syscall_puts("================================\n\n");
    
    syscall_puts("=== HEAP ALLOCATOR TEST ===\n\n");
    
    /* Initialize heap */
    heap_init();
    
    /* Test 1: Request 2 pages directly via syscall (baseline) */
    syscall_puts("Test 1: Direct syscall allocation (2 pages)\n");
    void *page1 = syscall_alloc_page();
    void *page2 = syscall_alloc_page();
    
    syscall_puts("Page 1: ");
    print_hex_user((unsigned int)page1);
    syscall_puts(" to ");
    print_hex_user((unsigned int)page1 + 4095);
    syscall_puts("\n");
    
    syscall_puts("Page 2: ");
    print_hex_user((unsigned int)page2);
    syscall_puts(" to ");
    print_hex_user((unsigned int)page2 + 4095);
    syscall_puts("\n\n");
    
    /* Test 2: First malloc - should trigger syscall for new page */
    syscall_puts("Test 2: malloc(4000) - should trigger syscall\n");
    void *block1 = malloc(4000);
    syscall_puts("Allocated block at: ");
    print_hex_user((unsigned int)block1);
    syscall_puts("\n\n");
    
    /* Test 3: Second malloc (30 bytes) - should NOT trigger syscall */
    syscall_puts("Test 3: malloc(30) - should reuse existing page\n");
    syscall_puts("(Watch: no '[HEAP] Requesting new page' message)\n");
    void *block2 = malloc(30);
    syscall_puts("Allocated block at: ");
    print_hex_user((unsigned int)block2);
    syscall_puts("\n");
    
    /* Verify block2 is close to block1 (same page) */
    unsigned int diff = (unsigned int)block2 - (unsigned int)block1;
    syscall_puts("Distance from first block: ");
    print_hex_user(diff);
    syscall_puts(" bytes\n");
    
    if (diff < 4096) {
        syscall_puts("SUCCESS: Both allocations in same page!\n");
    } else {
        syscall_puts("UNEXPECTED: Allocations in different pages\n");
    }
    syscall_puts("\n");
    
    /* Test 4: Show heap statistics */
    unsigned int total_pages, used_bytes, free_bytes;
    heap_stats(&total_pages, &used_bytes, &free_bytes);
    
    syscall_puts("Heap Statistics:\n");
    syscall_puts("  Total pages: ");
    syscall_putchar('0' + total_pages);
    syscall_puts("\n");
    syscall_puts("  Used bytes: ");
    print_hex_user(used_bytes);
    syscall_puts("\n");
    syscall_puts("  Free bytes: ");
    print_hex_user(free_bytes);
    syscall_puts("\n\n");
    
    syscall_puts("System ready.\n");
    
    /* Infinite loop (HLT is privileged, cannot use in Ring 3) */
    while(1) {
        /* Idle */
    }
}
