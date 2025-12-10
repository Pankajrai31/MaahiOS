// Syscall implementations are in pure assembly (user_syscalls.s)
// This file is empty - all functions implemented in assembly
#include "user_syscalls.h"

/**
 * Ring 3 Syscall Wrappers
 * 
 * Uses the OSDev-recommended approach:
 * - Load parameters directly into registers (EAX, EBX, ECX, EDX)
 * - Use volatile inline asm with explicit register constraints
 * - Trigger INT 0x80
 * - Let the interrupt handler read the registers
 * 
 * Key insight: Don't try to be clever with "memory", "cc" clobbers
 * Just load registers and call INT 0x80
 */

void syscall_putchar(char c) {
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_PUTCHAR), "b"(c)
        : "memory"
    );
}

void syscall_puts(const char* str) {
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_PUTS), "b"(str)
        : "memory"
    );
}

void syscall_putint(int num) {
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_PUTINT), "b"(num)
        : "memory"
    );
}

void syscall_exit(int code) {
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_EXIT), "b"(code)
        : "memory"
    );
}

void *syscall_alloc_page() {
    void *result;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_ALLOC_PAGE)
        : "memory"
    );
    return result;
}

void syscall_free_page(void *addr) {
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_FREE_PAGE), "b"(addr)
        : "memory"
    );
}

void syscall_clear() {
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_CLEAR)
        : "memory"
    );
}

void syscall_set_color(unsigned char fg, unsigned char bg) {
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_SET_COLOR), "b"(fg), "c"(bg)
        : "memory"
    );
}

int syscall_create_process(unsigned int entry_point) {
    int result;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_CREATE_PROCESS), "b"(entry_point)
        : "memory"
    );
    return result;
}

int syscall_get_orbit_address(void) {
    int result;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_GET_ORBIT_ADDR)
        : "memory"
    );
    return result;
}

/* Graphics Syscalls - Simple like printf(), no memory addresses! */

void gfx_putc(char c) {
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_GFX_PUTC), "b"((unsigned int)c)
        : "memory"
    );
}

void gfx_puts(const char *str) {
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_GFX_PUTS), "b"(str)
        : "memory"
    );
}

void gfx_clear(void) {
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_GFX_CLEAR)
        : "memory"
    );
}

void gfx_set_color(int fg, int bg) {
    // Convert color constants to RGB values
    static const unsigned int color_map[] = {
        0x00000000,  // BLACK
        0x00FFFFFF,  // WHITE
        0x00FF0000,  // RED
        0x0000FF00,  // GREEN
        0x000000FF,  // BLUE
        0x00FFFF00,  // YELLOW
        0x0000FFFF,  // CYAN
        0x00FF00FF   // MAGENTA
    };
    
    unsigned int fg_rgb = (fg >= 0 && fg < 8) ? color_map[fg] : color_map[1];
    unsigned int bg_rgb = (bg >= 0 && bg < 8) ? color_map[bg] : color_map[0];
    
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_GFX_SET_COLOR), "b"(fg_rgb), "c"(bg_rgb)
        : "memory"
    );
}

void syscall_fill_rect(int x, int y, int width, int height, unsigned int color) {
    // SIMPLIFIED: Pack width/height into one register to avoid stack issues
    // EDX = (height << 16) | (width & 0xFFFF)
    // ESI = color
    unsigned int packed_wh = ((unsigned int)height << 16) | ((unsigned int)width & 0xFFFF);
    
    asm volatile(
        "movl $23, %%eax\n\t"
        "movl %[x], %%ebx\n\t"
        "movl %[y], %%ecx\n\t"
        "movl %[packed], %%edx\n\t"
        "movl %[color], %%esi\n\t"
        "int $0x80\n\t"
        :
        : [x] "rm" (x), [y] "rm" (y), [packed] "rm" (packed_wh), [color] "rm" (color)
        : "eax", "ebx", "ecx", "edx", "esi", "memory"
    );
}

void syscall_draw_rect(int x, int y, int width, int height, unsigned int color) {
    asm volatile(
        "pushl %[color]\n\t"
        "pushl %[height]\n\t"
        "movl $24, %%eax\n\t"
        "movl %[x], %%ebx\n\t"
        "movl %[y], %%ecx\n\t"
        "movl %[width], %%edx\n\t"
        "int $0x80\n\t"
        "addl $8, %%esp\n\t"
        :
        : [x] "rm" (x), [y] "rm" (y), [width] "rm" (width), [height] "rm" (height), [color] "rm" (color)
        : "eax", "ebx", "ecx", "edx", "memory"
    );
}

void syscall_print_at(int x, int y, const char *str, unsigned int fg, unsigned int bg) {
    asm volatile(
        "pushl %[bg]\n\t"
        "pushl %[fg]\n\t"
        "movl $25, %%eax\n\t"
        "movl %[x], %%ebx\n\t"
        "movl %[y], %%ecx\n\t"
        "movl %[str], %%edx\n\t"
        "int $0x80\n\t"
        "addl $8, %%esp\n\t"
        :
        : [x] "rm" (x), [y] "rm" (y), [str] "rm" (str), [fg] "rm" (fg), [bg] "rm" (bg)
        : "eax", "ebx", "ecx", "edx", "memory"
    );
}

void syscall_gfx_clear_color(unsigned int rgb_color) {
    asm volatile(
        "movl $26, %%eax\n\t"
        "movl %[color], %%ebx\n\t"
        "int $0x80\n\t"
        :
        : [color] "rm" (rgb_color)
        : "eax", "ebx", "memory"
    );
}

void syscall_draw_bmp(int x, int y, unsigned int bmp_data_addr) {
    asm volatile(
        "movl $27, %%eax\n\t"
        "movl %[x], %%ebx\n\t"
        "movl %[y], %%ecx\n\t"
        "movl %[bmp], %%edx\n\t"
        "int $0x80\n\t"
        :
        : [x] "rm" (x), [y] "rm" (y), [bmp] "rm" (bmp_data_addr)
        : "eax", "ebx", "ecx", "edx", "memory"
    );
}

int syscall_mouse_get_x(void) {
    int result;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_MOUSE_GET_X)
        : "memory"
    );
    return result;
}

int syscall_mouse_get_y(void) {
    int result;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_MOUSE_GET_Y)
        : "memory"
    );
    return result;
}

unsigned int syscall_mouse_get_buttons(void) {
    unsigned int result;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_MOUSE_GET_BUTTONS)
        : "memory"
    );
    return result;
}

void syscall_yield(void) {
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_YIELD)
        : "memory"
    );
}

int syscall_mouse_get_irq_total(void) {
    int result;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_MOUSE_GET_IRQ_TOTAL)
        : "memory"
    );
    return result;
}

unsigned int syscall_get_pic_mask(void) {
    unsigned int result;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_GET_PIC_MASK)
        : "memory"
    );
    return result;
}

void syscall_re_enable_mouse(void) {
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_RE_ENABLE_MOUSE)
        : "memory"
    );
}

int syscall_poll_mouse(void) {
    int result;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_POLL_MOUSE)
        : "memory"
    );
    return result;
}

unsigned int syscall_read_pixel(int x, int y) {
    unsigned int result;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_READ_PIXEL), "b"(x), "c"(y)
        : "memory"
    );
    return result;
}
