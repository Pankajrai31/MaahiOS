#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>
#include "pmm.h"

// Page table entry flags
#define PAGE_PRESENT    0x1
#define PAGE_WRITE      0x2
#define PAGE_USER       0x4
#define PAGE_SIZE_4KB   4096

// Each page table has 1024 entries
#define ENTRIES_PER_TABLE 1024

// Identity map first 32MB (covers kernel, modules, VGA, etc.)
#define IDENTITY_MAP_SIZE 0x02000000  // 32MB

// Page directory and table structures
typedef uint32_t page_table_entry_t;
typedef uint32_t page_directory_entry_t;

// Paging functions
int paging_init(multiboot_info_t *mbi);
void paging_enable();
void paging_map_page(uint32_t *page_dir, uint32_t virt, uint32_t phys, uint32_t flags);
void identity_map_region(uint32_t *page_dir, uint32_t start, uint32_t end);

// VMM wrapper functions (simple wrappers for now, full VMM in Phase 3)
void *vmm_alloc_page();
void vmm_free_page(void *addr);

#endif
