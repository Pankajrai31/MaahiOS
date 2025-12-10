#ifndef SYSCALL_NUMBERS_H
#define SYSCALL_NUMBERS_H

// Syscall number definitions
// These are the identifiers used in EAX to specify which syscall to invoke

#define SYSCALL_PUTCHAR     1   // putchar(char c) - Print single character
#define SYSCALL_PUTS        2   // puts(const char* str) - Print null-terminated string
#define SYSCALL_PUTINT      3   // putint(int num) - Print integer
#define SYSCALL_EXIT        4   // exit(int code) - Terminate program
#define SYSCALL_WRITE       5   // write(int fd, char* buf, int size) - Write to file descriptor
#define SYSCALL_ALLOC_PAGE  6   // alloc_page() - Allocate a physical page (4KB)
#define SYSCALL_FREE_PAGE   7   // free_page(void* addr) - Free a physical page
#define SYSCALL_CLEAR       8   // clear() - Clear screen
#define SYSCALL_SET_COLOR   9   // set_color(fg, bg) - Set text color
#define SYSCALL_DRAW_RECT   10  // draw_rect(x, y, width, height, color) - Draw filled rectangle
#define SYSCALL_GRAPHICS_MODE 11 // graphics_mode() - Switch to 320x200 graphics mode
#define SYSCALL_PUT_PIXEL   12  // put_pixel(x, y, color) - Draw a pixel
#define SYSCALL_CLEAR_GFX   13  // clear_gfx(color) - Clear graphics screen
#define SYSCALL_PRINT_AT    14  // print_at(x, y, str) - Print string at position
#define SYSCALL_SET_CURSOR  15  // set_cursor(x, y) - Set cursor position
#define SYSCALL_DRAW_BOX    16  // draw_box(x, y, width, height) - Draw box border
#define SYSCALL_CREATE_PROCESS 17  // create_process(entry_point) - Create new process
#define SYSCALL_GET_ORBIT_ADDR 18  // get_orbit_address() - Get orbit module address

// Graphics syscalls (Simple text-mode-like API - no addresses!)
#define SYSCALL_GFX_PUTC        19  // gfx_putc(char c) - Print character at cursor
#define SYSCALL_GFX_PUTS        20  // gfx_puts(str) - Print string at cursor
#define SYSCALL_GFX_CLEAR       21  // gfx_clear() - Clear screen to black
#define SYSCALL_GFX_SET_COLOR   22  // gfx_set_color(fg, bg) - Set text colors
#define SYSCALL_GFX_FILL_RECT   23  // gfx_fill_rect(x, y, w, h, color) - Draw filled rectangle
#define SYSCALL_GFX_DRAW_RECT   24  // gfx_draw_rect(x, y, w, h, color) - Draw rectangle outline
#define SYSCALL_GFX_PRINT_AT    25  // gfx_print_at(x, y, str, fg, bg) - Print at position
#define SYSCALL_GFX_CLEAR_COLOR 26  // gfx_clear_color(rgb) - Clear screen to RGB color
#define SYSCALL_GFX_DRAW_BMP    27  // gfx_draw_bmp(x, y, bmp_data) - Draw BMP image at position

// Mouse syscalls
#define SYSCALL_MOUSE_GET_X     28  // mouse_get_x() - Get mouse X position
#define SYSCALL_MOUSE_GET_Y     29  // mouse_get_y() - Get mouse Y position
#define SYSCALL_MOUSE_GET_BUTTONS 30 // mouse_get_buttons() - Get button states

// Scheduler syscalls
#define SYSCALL_YIELD           31  // yield() - Yield CPU to scheduler

// Debug syscalls
#define SYSCALL_MOUSE_GET_IRQ_TOTAL 32  // mouse_get_irq_total() - Get total IRQ12 count
#define SYSCALL_GET_PIC_MASK        33  // get_pic_mask() - Get PIC mask register
#define SYSCALL_RE_ENABLE_MOUSE     34  // re_enable_mouse() - Re-enable IRQ12
#define SYSCALL_POLL_MOUSE          35  // poll_mouse() - Manually poll 8042 for mouse data if IRQ12 stopped
#define SYSCALL_READ_PIXEL          36  // read_pixel(x, y) - Read pixel from framebuffer for cursor compositor

// VGA Color constants (for reference)
// Foreground/Background colors: 0-15
// 0=Black, 1=Blue, 2=Green, 3=Cyan, 4=Red, 5=Magenta, 6=Brown, 7=Light Gray
// 8=Dark Gray, 9=Light Blue, 10=Light Green, 11=Light Cyan, 12=Light Red
// 13=Light Magenta, 14=Yellow, 15=White

#endif // SYSCALL_NUMBERS_H
