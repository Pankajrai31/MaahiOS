/*
 * Double Buffer Manager Implementation
 */

#include "double_buffer.h"
#include "../../../../system/syscalls/user/user_syscalls.h"

/* Screen dimensions */
static int screen_width = 0;
static int screen_height = 0;

/* Buffer pointers */
static uint32_t *back_buffer = 0;
static uint32_t *front_buffer = 0;

/* Buffer size in pixels */
static int buffer_size = 0;

/**
 * Fast memory copy (optimized for 32-bit aligned data)
 */
static void fast_memcpy(uint32_t *dest, const uint32_t *src, int count) {
    for (int i = 0; i < count; i++) {
        dest[i] = src[i];
    }
}

/**
 * Initialize double buffering
 */
int double_buffer_init(int width, int height, uint32_t *framebuffer) {
    if (!framebuffer || width <= 0 || height <= 0) {
        return -1;
    }
    
    screen_width = width;
    screen_height = height;
    buffer_size = width * height;
    
    /* Front buffer is the physical framebuffer */
    front_buffer = framebuffer;
    
    /* Allocate back buffer using syscall_alloc_page */
    /* Need: width * height * 4 bytes (32-bit color) */
    int bytes_needed = buffer_size * 4;
    int pages_needed = (bytes_needed + 4095) / 4096;  /* Round up to pages */
    
    /* Allocate pages for back buffer */
    syscall_puts("[DOUBLEBUF] Allocating ");
    syscall_putint(pages_needed);
    syscall_puts(" pages for back buffer...\n");
    
    back_buffer = 0;
    for (int i = 0; i < pages_needed; i++) {
        void *page = syscall_alloc_page();
        if (!page) {
            syscall_puts("[DOUBLEBUF] FAILED at page ");
            syscall_putint(i);
            syscall_puts("\n");
            return -1;  /* Allocation failed */
        }
        
        /* First page becomes our back buffer base */
        if (i == 0) {
            back_buffer = (uint32_t *)page;
        }
        
        /* Progress indicator every 100 pages */
        if ((i + 1) % 100 == 0) {
            syscall_puts("[DOUBLEBUF] Allocated ");
            syscall_putint(i + 1);
            syscall_puts(" / ");
            syscall_putint(pages_needed);
            syscall_puts(" pages\n");
        }
    }
    
    syscall_puts("[DOUBLEBUF] All pages allocated\n");
    
    if (!back_buffer) {
        return -1;
    }
    
    /* Clear back buffer to black */
    double_buffer_clear(0x000000);
    
    return 0;
}

/**
 * Get back buffer pointer
 */
uint32_t* double_buffer_get_back(void) {
    return back_buffer;
}

/**
 * Get front buffer pointer
 */
uint32_t* double_buffer_get_front(void) {
    return front_buffer;
}

/**
 * Swap buffers - copy back to front
 */
void double_buffer_swap(void) {
    if (!back_buffer || !front_buffer) {
        return;
    }
    
    /* Full screen copy */
    fast_memcpy(front_buffer, back_buffer, buffer_size);
}

/**
 * Clear back buffer
 */
void double_buffer_clear(uint32_t color) {
    if (!back_buffer) {
        return;
    }
    
    for (int i = 0; i < buffer_size; i++) {
        back_buffer[i] = color;
    }
}

/**
 * Get screen width
 */
int double_buffer_get_width(void) {
    return screen_width;
}

/**
 * Get screen height
 */
int double_buffer_get_height(void) {
    return screen_height;
}

/**
 * Mark a region as dirty
 */
void double_buffer_mark_dirty(int x, int y, int width, int height) {
#if USE_DIRTY_RECT
    dirty_rect_add(x, y, width, height);
#else
    /* When disabled, do nothing - full screen is always copied */
    (void)x; (void)y; (void)width; (void)height;
#endif
}
