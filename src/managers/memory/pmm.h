#ifndef PMM_H
#define PMM_H

#include <stdint.h>

// Multiboot info structure (simplified)
typedef struct {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
} multiboot_info_t;

typedef struct {
    uint32_t mod_start;
    uint32_t mod_end;
    uint32_t string;
    uint32_t reserved;
} multiboot_module_t;

// Page size constants
#define PAGE_SIZE 4096
#define PAGES_PER_BYTE 8

// PMM Functions
int pmm_init(multiboot_info_t *mbi);
void pmm_mark_region_used(uint32_t start, uint32_t end);
void *pmm_alloc_page();
void pmm_free_page(void *addr);
void pmm_print_stats();
uint32_t pmm_get_free_pages();
uint32_t pmm_get_total_pages();

#endif
