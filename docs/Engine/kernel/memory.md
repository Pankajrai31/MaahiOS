# Kernel Memory Management

## Components

| File | Purpose |
|------|---------|
| managers/memory/pmm.c/.h | Physical Memory Manager — bitmap allocator |
| managers/memory/paging.c/.h | Page directory/table management |
| managers/memory/kheap.c/.h | Kernel heap (bump + free-list, 4MB) |

## Memory Map

| Region | Start | Size | Notes |
|--------|-------|------|-------|
| Kernel text/data/bss | 0x00100000 | Variable | Loaded by GRUB |
| Kernel heap | 0x01000000 | 4 MB | KHEAP_START |
| Identity-mapped | 0x00000000 | 128 MB | Kernel-accessible |
| User process base | 0x10000000 | Per-process | Code + BSS |
| SHM regions | Dynamic | Up to 64 | PMM-allocated pages |
| Framebuffer | Hardware | ~4 MB | Identity-mapped at boot |

## PMM API
- `pmm_alloc_page()` — returns physical address of free 4KB page
- `pmm_free_page(addr)` — returns page to free pool
- Uses bitmap to track all physical pages

## Paging API
- `paging_init()` — sets up page directory, identity maps kernel space
- `paging_map_page(virt, phys, flags)` — maps single page
- User processes get their own page directory

## Kernel Heap API
- `kheap_alloc(size)` — allocate from 4MB kernel heap
- `kheap_free(ptr)` — return to free list
- NOT used for user-space allocations

## Process Memory Allocation
- `process_create_from_memory()` uses MEX header BSS size
- Allocates: `binary_size + max(bss_size, MIN_BSS_RESERVE)` bytes, page-aligned
- User code mapped at 0x10000000

## Known Issues
*(Agents add issues here as they find them)*
