# Memory Management Documentation

## Overview

MaahiOS implements a multi-layer memory management system:
1. **PMM (Physical Memory Manager)** - Manages physical page allocation
2. **Paging** - Virtual memory with identity mapping
3. **Kernel Heap (kheap)** - Bump allocator for kernel
4. **User Heap (heap)** - Free-list allocator for user processes

---

## pmm.c / pmm.h

**Purpose:** Physical Memory Manager using bitmap allocation.

### Constants
```c
#define PAGE_SIZE 4096
#define PAGES_PER_BYTE 8
```

### Global State
```c
static uint32_t *bitmap = 0;      // Allocation bitmap
static uint32_t total_pages = 0;   // Total pages available
static uint32_t used_pages = 0;    // Currently used pages
static uint32_t bitmap_size = 0;   // Bitmap size in uint32_t units
static uint32_t memory_start = 0;  // Start of managed memory (1MB)
```

### Structures

#### `multiboot_info_t`
```c
typedef struct {
    uint32_t flags;
    uint32_t mem_lower;     // Memory below 1MB (KB)
    uint32_t mem_upper;     // Memory above 1MB (KB)
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;    // Number of modules loaded
    uint32_t mods_addr;     // Pointer to module table
} multiboot_info_t;
```

#### `multiboot_module_t`
```c
typedef struct {
    uint32_t mod_start;
    uint32_t mod_end;
    uint32_t string;
    uint32_t reserved;
} multiboot_module_t;
```

### Key Functions

| Function | Description |
|----------|-------------|
| `pmm_init(mbi)` | Initialize PMM from multiboot info |
| `pmm_mark_region_used(start, end)` | Mark address range as used |
| `pmm_alloc_page()` | Allocate a 4KB page |
| `pmm_free_page(addr)` | Free a page |
| `pmm_print_stats()` | Print memory statistics |
| `pmm_get_free_pages()` | Get number of free pages |
| `pmm_get_total_pages()` | Get total page count |

### Bitmap Operations
```c
static void bitmap_set(uint32_t page);    // Mark as used
static void bitmap_clear(uint32_t page);  // Mark as free
static int bitmap_test(uint32_t page);    // Check if used
```

### Initialization Process
1. Calculate total memory from `mem_upper`
2. Set `memory_start` to 1MB (0x00100000)
3. Calculate total pages and bitmap size
4. Place bitmap after kernel and modules
5. Mark kernel region as used
6. Mark modules as used
7. Mark bitmap itself as used

### Issues Identified

1. **Linear Search for Free Pages (Lines 130-143)**
   ```c
   for (uint32_t page = 0; page < total_pages; page++) {
       if (!bitmap_test(page)) { ... }
   }
   ```
   - **Issue:** O(n) search on every allocation.
   - **Suggestion:** Maintain a free list or hint for next free page.

2. **No Coalescing on Free**
   - Adjacent free pages are not merged.
   - Not critical for page allocation but could fragment.

3. **Missing `kernel_end` Symbol Check**
   - Uses `extern uint32_t kernel_end` but doesn't verify it exists.
   - **Suggestion:** Add runtime check.

---

## paging.c / paging.h

**Purpose:** Virtual memory management with page directories and tables.

### Constants
```c
#define PAGE_PRESENT    0x1
#define PAGE_WRITE      0x2
#define PAGE_USER       0x4
#define PAGE_SIZE_4KB   4096
#define ENTRIES_PER_TABLE 1024
#define IDENTITY_MAP_SIZE 0x02000000  // 32MB
```

### Global State
```c
uint32_t *kernel_page_directory = 0;  // Global page directory
static uint32_t identity_map_end = 0;  // End of identity-mapped region
```

### Key Functions

| Function | Description |
|----------|-------------|
| `paging_init(mbi)` | Initialize paging subsystem |
| `paging_enable()` | Load CR3 and enable paging |
| `paging_map_page(pd, virt, phys, flags)` | Map single page |
| `identity_map_region(pd, start, end)` | Identity map address range |
| `paging_map_mmio_region(phys, size)` | Map MMIO region |
| `vmm_alloc_page()` | Allocate virtual page |
| `vmm_free_page(addr)` | Free virtual page |

### Page Table Structure
- 1024-entry page directory (4KB)
- Each entry points to a page table
- 1024 entries per page table
- 4GB virtual address space coverage

### Identity Mapping
- First 128MB is identity mapped (virtual = physical)
- Kernel and modules are in this region
- User processes can access this memory

### Issues Identified

1. **All Pages Have USER Flag (Line 63)**
   ```c
   paging_map_page(page_dir, addr, addr, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
   ```
   - **Issue:** Kernel pages should not have USER flag.
   - **Suggestion:** Create separate kernel mapping without PAGE_USER.

2. **Fixed 128MB Identity Map (Line 158)**
   ```c
   identity_map_end = 0x08000000;  // 128MB
   ```
   - **Issue:** May not match actual memory available.
   - **Suggestion:** Base on actual memory from PMM.

3. **VMM Functions Are Simple Wrappers (Lines 175-197)**
   - Currently just call PMM directly.
   - **Suggestion:** Implement proper virtual address allocation.

4. **TLB Flush Method (Lines 84, 223)**
   ```c
   asm volatile("mov %%cr3, %%eax; mov %%eax, %%cr3" ::: "eax");
   ```
   - Works but could use `invlpg` for single page invalidation.

---

## kheap.c / kheap.h

**Purpose:** Kernel-space heap allocator using bump allocation.

### Global State
```c
static unsigned int heap_start = 0;
static unsigned int heap_end = 0;
static unsigned int heap_current = 0;
```

### Heap Boundaries
- **Start:** First free page from PMM (after kernel structures)
- **End:** 128MB (identity map limit)

### Key Functions

| Function | Description |
|----------|-------------|
| `kheap_init()` | Initialize kernel heap |
| `kmalloc(size)` | Allocate memory |
| `kmalloc_aligned(size, alignment)` | Allocate aligned memory |
| `kfree(ptr)` | Free memory (no-op) |
| `kheap_stats(total, used, free)` | Get heap statistics |

### Allocation Algorithm
```c
void* kmalloc(size_t size) {
    // Align to 16 bytes
    size = (size + 15) & ~15;
    
    // Check space
    if (heap_current + size > heap_end)
        return 0;
    
    // Bump allocate
    void *ptr = (void *)heap_current;
    heap_current += size;
    
    return ptr;
}
```

### Issues Identified

1. **No Free Implementation (Lines 98-101)**
   ```c
   void kfree(void* ptr) {
       /* Bump allocator doesn't free - ignore */
       (void)ptr;
   }
   ```
   - **Issue:** Memory leaks over time.
   - **Suggestion:** Implement proper free-list allocator for production.

2. **Missing krealloc and kcalloc (from header)**
   - Header declares these functions but they're not implemented.
   - **Suggestion:** Implement or remove from header.

3. **Static Variable in kmalloc_aligned (Line 85)**
   ```c
   static unsigned int last_page_allocated_aligned = 0;
   ```
   - Separate tracking from `kmalloc` could cause issues.
   - **Suggestion:** Use shared tracking.

---

## heap.c / heap.h

**Purpose:** User-space heap allocator using free-list algorithm.

### Block Header
```c
typedef struct block_header {
    size_t size;                    // Usable data size
    int is_free;                    // 1=free, 0=used
    struct block_header *next;      // Next block
} block_header_t;

#define BLOCK_HEADER_SIZE sizeof(block_header_t)
#define PAGE_SIZE 4096
#define ALIGN_SIZE 8
```

### Key Functions

| Function | Description |
|----------|-------------|
| `heap_init()` | Initialize heap allocator |
| `malloc(size)` | Allocate memory |
| `free(ptr)` | Free memory |
| `heap_stats(total, used, free)` | Get statistics |

### Allocation Algorithm
- First-fit search through free list
- Block splitting when block is larger than needed
- Coalescing with next block on free

### Issues Identified

1. **Missing Coalesce with Previous (Line 150-151)**
   ```c
   /* TODO: Coalesce with previous block (requires doubly-linked list) */
   ```
   - Only coalesces with next block, not previous.
   - **Suggestion:** Implement doubly-linked list for full coalescing.

2. **Recursive Allocation on Expand (Lines 127-131)**
   ```c
   expand_heap();
   if (heap_start) {
       return malloc(size);  // Recursive call
   }
   ```
   - Could cause stack overflow with many failed expansions.
   - **Suggestion:** Use loop instead of recursion.

3. **No Maximum Size Check**
   - Could request very large allocations.
   - **Suggestion:** Add maximum allocation size limit.

4. **Debug Output in Production Code (Lines 40, 70-77)**
   - `syscall_puts()` called for allocation logging.
   - **Suggestion:** Make debug output conditional.
