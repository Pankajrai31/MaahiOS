/**
 * MaahiOS User-Space Heap Allocator - malloc/free
 *
 * Description:
 *   A first-fit free-list allocator for user-space processes.
 *   Acquires pages from the kernel via SYS_MEM_ALLOC_PAGE (4KB)
 *   and manages sub-page blocks internally. Supports real free()
 *   with adjacent-block coalescing to avoid fragmentation.
 *
 *   Block layout:
 *     [header (8 bytes)] [user payload ...] [header] [payload] ...
 *     header = { uint32_t size; uint32_t flags; }
 *     size   = payload bytes (excludes header)
 *     flags  = BLOCK_FREE (1) or 0 (used)
 *
 * Author: MaahiOS Team
 * Date:   March 2026
 */

#include "../libmemory/libmemory.h"
#include <stdint.h>

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define PAGE_SIZE       4096
#define HEADER_SIZE     8       /* sizeof(block_header_t), kept as constant */
#define MIN_BLOCK_SIZE  16      /* Minimum payload to avoid tiny splinters */
#define BLOCK_FREE      1
#define BLOCK_USED      0
#define HEAP_MAX_PAGES  256     /* Max 1 MB of heap pages tracked */

/*=============================================================================
 * BLOCK HEADER — sits just before every user payload
 *===========================================================================*/

typedef struct block_header {
    uint32_t size;              /* Payload size in bytes (after this header)   */
    uint32_t flags;             /* BLOCK_FREE or BLOCK_USED                    */
} block_header_t;

/*=============================================================================
 * HEAP STATE
 *===========================================================================*/

/* Pages we've acquired from the kernel */
static uint32_t heap_pages[HEAP_MAX_PAGES];
static uint32_t heap_page_sizes[HEAP_MAX_PAGES]; /* bytes per page/chunk */
static int      heap_page_count = 0;

/*=============================================================================
 * INTERNAL: Get next adjacent block within the same page
 *===========================================================================*/

static block_header_t *next_block(block_header_t *blk, uint32_t page_base,
                                  uint32_t page_size) {
    uint32_t next_addr = (uint32_t)blk + HEADER_SIZE + blk->size;
    uint32_t page_end  = page_base + page_size;
    if (next_addr + HEADER_SIZE > page_end) return (block_header_t *)0;
    return (block_header_t *)next_addr;
}

/*=============================================================================
 * INTERNAL: Try to allocate from an existing page
 *===========================================================================*/

static void *try_alloc_from_page(int page_idx, uint32_t size) {
    uint32_t base = heap_pages[page_idx];
    uint32_t pg_size = heap_page_sizes[page_idx];
    block_header_t *blk = (block_header_t *)base;
    uint32_t page_end = base + pg_size;

    while ((uint32_t)blk + HEADER_SIZE <= page_end) {
        if (blk->flags == BLOCK_FREE && blk->size >= size) {
            /* Found a free block big enough */

            /* Split if remainder is worth keeping */
            uint32_t remainder = blk->size - size;
            if (remainder >= HEADER_SIZE + MIN_BLOCK_SIZE) {
                /* Create a new free block after the allocated one */
                block_header_t *split = (block_header_t *)
                    ((uint32_t)blk + HEADER_SIZE + size);
                split->size  = remainder - HEADER_SIZE;
                split->flags = BLOCK_FREE;
                blk->size    = size;
            }

            blk->flags = BLOCK_USED;
            return (void *)((uint32_t)blk + HEADER_SIZE);
        }

        /* Move to next block */
        block_header_t *nxt = next_block(blk, base, pg_size);
        if (!nxt) break;
        blk = nxt;
    }

    return (void *)0;
}

/*=============================================================================
 * INTERNAL: Acquire a new page from the kernel
 *===========================================================================*/

static int acquire_page(uint32_t min_bytes) {
    if (heap_page_count >= HEAP_MAX_PAGES) return -1;

    /* Calculate how many pages we need. Always at least 1 page.
     * For large allocations, request enough contiguous pages. */
    uint32_t need = min_bytes + HEADER_SIZE;
    uint32_t pages = (need + PAGE_SIZE - 1) / PAGE_SIZE;
    uint32_t alloc_bytes = pages * PAGE_SIZE;

    /* Route through libmemory (proper layering — never call SYS_MEM_* directly) */
    void *page;
    if (pages == 1) {
        page = libmem_alloc_page();
    } else {
        page = libmem_alloc(alloc_bytes);
    }
    if (!page) return -1;

    /* Initialize as one big free block */
    block_header_t *blk = (block_header_t *)page;
    blk->size  = alloc_bytes - HEADER_SIZE;
    blk->flags = BLOCK_FREE;

    int idx = heap_page_count;
    heap_pages[idx]      = (uint32_t)page;
    heap_page_sizes[idx] = alloc_bytes;
    heap_page_count++;

    return idx;
}

/*=============================================================================
 * PUBLIC: malloc
 *===========================================================================*/

void *malloc(unsigned int size) {
    if (size == 0) return (void *)0;

    /* Align size up to 4 bytes for proper alignment */
    size = (size + 3) & ~3u;

    /* First-fit: search existing pages */
    for (int i = 0; i < heap_page_count; i++) {
        void *ptr = try_alloc_from_page(i, size);
        if (ptr) return ptr;
    }

    /* No room — acquire a new page and allocate from it */
    int idx = acquire_page(size);
    if (idx < 0) return (void *)0;

    return try_alloc_from_page(idx, size);
}

/*=============================================================================
 * PUBLIC: free — coalescing free-list implementation
 *===========================================================================*/

void free(void *ptr) {
    if (!ptr) return;

    /* Get the block header just before the user pointer */
    block_header_t *blk = (block_header_t *)((uint32_t)ptr - HEADER_SIZE);
    blk->flags = BLOCK_FREE;

    /* Find which page this block belongs to, then coalesce */
    uint32_t addr = (uint32_t)ptr;
    for (int i = 0; i < heap_page_count; i++) {
        uint32_t base = heap_pages[i];
        uint32_t end  = base + heap_page_sizes[i];
        if (addr >= base && addr < end) {
            /* Walk all blocks in this page, merging adjacent free blocks */
            block_header_t *cur = (block_header_t *)base;
            while (cur) {
                block_header_t *nxt = next_block(cur, base, heap_page_sizes[i]);
                if (!nxt) break;
                if (cur->flags == BLOCK_FREE && nxt->flags == BLOCK_FREE) {
                    /* Merge: absorb nxt into cur */
                    cur->size += HEADER_SIZE + nxt->size;
                    /* Don't advance — check if we can merge again */
                    continue;
                }
                cur = nxt;
            }
            return;
        }
    }

    /* ptr not in our heap — nothing we can do */
}
