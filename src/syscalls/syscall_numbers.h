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

// VGA Color constants (for reference)
// Foreground/Background colors: 0-15
// 0=Black, 1=Blue, 2=Green, 3=Cyan, 4=Red, 5=Magenta, 6=Brown, 7=Light Gray
// 8=Dark Gray, 9=Light Blue, 10=Light Green, 11=Light Cyan, 12=Light Red
// 13=Light Magenta, 14=Yellow, 15=White

#endif // SYSCALL_NUMBERS_H
