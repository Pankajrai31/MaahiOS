# MaahiOS Code Issues and Improvement Suggestions

This document provides a comprehensive list of issues identified in the codebase and suggestions for fixing them.

> **Note:** Line number references are approximate and may change as the code evolves. Use them as guidance to locate the relevant code sections.

---

## Critical Issues

### 1. Security: User Space Accessing Kernel Memory

**Location:** `src/orbit/orbit.c` (Lines 103-105)

**Issue:**
```c
volatile int *kernel_mouse_x = (volatile int *)0x00118040;
volatile int *kernel_mouse_y = (volatile int *)0x00118044;
volatile int *kernel_irq_total = (volatile int *)0x0011804C;
```

Ring 3 code directly accesses kernel memory addresses. This works because all pages have `PAGE_USER` flag set.

**Fix:**
1. Remove `PAGE_USER` flag from kernel pages in `paging.c`:
```c
void identity_map_kernel_region(uint32_t *page_dir, uint32_t start, uint32_t end) {
    for (uint32_t addr = start; addr < end; addr += PAGE_SIZE_4KB) {
        paging_map_page(page_dir, addr, addr, PAGE_PRESENT | PAGE_WRITE);  // No PAGE_USER
    }
}
```
2. Remove direct memory access from orbit.c and use syscalls exclusively.

---

### 2. Memory Leak: kfree() Does Nothing

**Location:** `src/lib/kheap.c` (Lines 98-101)

**Issue:**
```c
void kfree(void* ptr) {
    /* Bump allocator doesn't free - ignore */
    (void)ptr;
}
```

Kernel memory is never freed, causing eventual exhaustion.

**Fix:** Implement a proper free-list allocator:
```c
typedef struct block_header {
    size_t size;
    int is_free;
    struct block_header *next;
} block_header_t;

void kfree(void* ptr) {
    if (!ptr) return;
    block_header_t *block = (block_header_t*)((char*)ptr - sizeof(block_header_t));
    block->is_free = 1;
    // Coalesce with next block if free
}
```

---

### 3. Double EOI to PIC

**Location:** `src/drivers/mouse.c` (Lines 204-205) and `src/managers/interrupt/interrupt_stubs.s` (Lines 216-218)

**Issue:** EOI sent in both handler and stub:
```c
// In mouse_handler()
outb(0xA0, 0x20);
outb(0x20, 0x20);
```
```assembly
# In irq12_stub
movb $0x20, %al
outb %al, $0xA0
outb %al, $0x20
```

**Fix:** Remove EOI from `mouse_handler()`:
```c
void mouse_handler() {
    // ... packet handling ...
    // EOI sent by assembly stub
}
```

---

## High Priority Issues

### 4. Hardcoded Framebuffer Address

**Location:** `src/kernel.c` (Line 130)

**Issue:**
```c
uint32_t fb_addr = 0xFD000000;  // QEMU default BGA framebuffer
```

This may not work on different VM configurations or hardware.

**Fix:**
```c
uint32_t fb_addr = bga_get_framebuffer_addr();  // Get from PCI scan
if (fb_addr == 0) {
    // Error handling
}
```

---

### 5. Incomplete Font Tables

**Location:** `src/drivers/bga.c` (Lines 18-146) and `src/drivers/vbe.c`

**Issue:** Many ASCII characters missing from font tables.

**Fix:** Create a complete font file or use a font generator:
```c
// font_8x16.h - generate from a standard VGA font
static const uint8_t font_8x16[256][16] = {
    // Complete character set
};
```

---

### 6. Missing krealloc and kcalloc

**Location:** `src/lib/kheap.h` declares but doesn't implement.

**Fix:** Add implementations:
```c
void* krealloc(void* ptr, size_t new_size) {
    if (!ptr) return kmalloc(new_size);
    void *new_ptr = kmalloc(new_size);
    if (!new_ptr) return 0;
    // Copy old data - need to track old size
    kfree(ptr);
    return new_ptr;
}

void* kcalloc(size_t count, size_t size) {
    size_t total = count * size;
    void *ptr = kmalloc(total);
    if (ptr) {
        for (size_t i = 0; i < total; i++) {
            ((char*)ptr)[i] = 0;
        }
    }
    return ptr;
}
```

---

### 7. IconType Not Defined in libgui.h

**Location:** `src/libgui/icons.h` uses `IconType` but it's not in `libgui.h`.

**Fix:** Add to `libgui.h`:
```c
/* Icon Types */
typedef enum {
    ICON_PROCESS,
    ICON_DISK,
    ICON_FILES,
    ICON_NOTEBOOK
} IconType;
```

---

## Medium Priority Issues

### 8. Duplicated I/O Functions

**Issue:** `outb()`, `inb()`, `outw()`, `inw()`, `outl()`, `inl()` duplicated in:
- `src/kernel.c`
- `src/drivers/graphics.c`
- `src/drivers/bga.c`
- `src/drivers/vbe.c`
- `src/drivers/mouse.c`
- `src/drivers/pci.c`
- `src/drivers/usb.c`
- `src/managers/ring3/ring3.c`
- `src/managers/irq/irq_manager.c`
- `src/managers/timer/pit.h`

**Fix:** Create shared header `src/lib/io.h`:
```c
#ifndef IO_H
#define IO_H

#include <stdint.h>

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// ... outw, inw, outl, inl ...

#endif // IO_H
```

---

### 9. No VGA Scrolling

**Location:** `src/drivers/vga.c` (Lines 38-48)

**Issue:** Text stops at line 24 instead of scrolling.

**Fix:**
```c
static void vga_scroll(void) {
    // Move lines 1-24 to 0-23
    for (int i = 0; i < VGA_WIDTH * (VGA_HEIGHT - 1); i++) {
        vga[i] = vga[i + VGA_WIDTH];
    }
    // Clear last line
    for (int i = 0; i < VGA_WIDTH; i++) {
        vga[(VGA_HEIGHT - 1) * VGA_WIDTH + i] = (vga_current_attr << 8) | ' ';
    }
}

void vga_print(const char *s) {
    while (*s) {
        // ...
        if (vga_y >= VGA_HEIGHT) {
            vga_scroll();
            vga_y = VGA_HEIGHT - 1;
        }
        // ...
    }
}
```

---

### 10. Recursive malloc on Heap Expansion

**Location:** `src/lib/heap.c` (Lines 127-131)

**Issue:**
```c
expand_heap();
if (heap_start) {
    return malloc(size);  // Recursive
}
```

**Fix:** Use loop:
```c
while (suitable_block == NULL) {
    expand_heap();
    if (!heap_start) return 0;  // Failed
    
    // Search again
    current = heap_start;
    while (current) {
        if (current->is_free && current->size >= size) {
            suitable_block = current;
            break;
        }
        current = current->next;
    }
}
```

---

### 11. Hardcoded Screen Bounds in Mouse

**Location:** `src/drivers/mouse.c` (Lines 153-155)

**Issue:**
```c
if (mouse_x > 1023) mouse_x = 1023;
if (mouse_y > 767)  mouse_y = 767;
```

**Fix:**
```c
static int screen_width = 1024;
static int screen_height = 768;

void mouse_set_bounds(int width, int height) {
    screen_width = width;
    screen_height = height;
}

// In push_packet:
if (mouse_x >= screen_width) mouse_x = screen_width - 1;
if (mouse_y >= screen_height) mouse_y = screen_height - 1;
```

---

### 12. Hardcoded Colors in Syscall Handler

**Location:** `src/syscalls/syscall_handler.c` (Lines 287-289, 392-398)

**Issue:** Colors forced to white/black ignoring actual parameters.

**Fix:**
```c
case SYSCALL_PRINT_AT:
    {
        int x = (int)arg1;
        int y = (int)arg2;
        const char *str = (const char*)arg3;
        // Read actual colors from ESI and stack
        uint32_t fg = arg4_esi;
        uint32_t *stack_ptr = (uint32_t *)user_esp;
        uint32_t bg = stack_ptr[0];
        bga_print_at(x, y, str, fg, bg);
    }
    break;
```

---

## Low Priority Issues

### 13. Debug Serial Output Everywhere

**Issue:** Heavy serial debugging in production code paths.

**Fix:** Add conditional compilation:
```c
#define DEBUG_SERIAL 0

#if DEBUG_SERIAL
    serial_print("[DEBUG] ...\n");
#endif
```

---

### 14. USB Driver Incomplete

**Location:** `src/drivers/usb.c`

**Issue:** `usb_tablet_get_report()` returns cached values, not real input.

**Fix:** Either implement properly or remove:
```c
int usb_tablet_get_report(int32_t *x, int32_t *y, uint8_t *buttons) {
    // TODO: Implement USB interrupt transfer
    // For now, return error
    return 0;
}
```

---

### 15. Unused Context Switch Code

**Location:** `src/managers/scheduler/switch_osdev.s`

**Issue:** `switch_to_task` function never called.

**Fix:** Either integrate into scheduler or remove file.

---

### 16. Fixed Window/Button Pools

**Location:** `src/libgui/window.c` (Line 9), `src/libgui/controls.c` (Line 8)

**Issue:** Static arrays limit maximum windows/buttons.

**Fix:** Use dynamic allocation:
```c
GUI_Window* gui_create_window(...) {
    GUI_Window *win = (GUI_Window *)malloc(sizeof(GUI_Window));
    if (!win) return NULL;
    // ...
    return win;
}
```

---

### 17. Tick Counter Overflow

**Location:** `src/managers/timer/pit.c` (Lines 15, 52-57)

**Issue:** `pit_ticks` wraps after ~49 days at 1000Hz.

**Fix:** Use 64-bit counter:
```c
static volatile uint64_t pit_ticks = 0;

void pit_wait(unsigned int ticks) {
    uint64_t end_tick = pit_ticks + ticks;
    while (pit_ticks < end_tick) {
        asm volatile("pause");
    }
}
```

---

## Code Quality Suggestions

### 1. Create Common Header Files
- `io.h` - Port I/O functions
- `types.h` - Common type definitions
- `debug.h` - Debug macros

### 2. Add Error Codes
Create standard error codes for all functions:
```c
#define E_SUCCESS    0
#define E_NOMEM     -1
#define E_INVALID   -2
#define E_NOTFOUND  -3
```

### 3. Add Documentation Comments
Use consistent documentation style:
```c
/**
 * @brief Allocate a physical memory page
 * @return Physical address of allocated page, or NULL on failure
 */
void* pmm_alloc_page(void);
```

### 4. Add Unit Tests
Create test framework for critical functions:
- Memory allocation tests
- String handling tests
- Data structure tests

### 5. Implement Proper Logging
Replace ad-hoc serial prints with logging system:
```c
#define LOG_DEBUG 0
#define LOG_INFO  1
#define LOG_WARN  2
#define LOG_ERROR 3

void log(int level, const char *msg, ...);
```
