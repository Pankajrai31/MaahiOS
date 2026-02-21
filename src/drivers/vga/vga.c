/**
 * MaahiOS VGA Text Mode Driver
 * Early boot console output (works before any driver init)
 * 
 * Note: This driver does NOT register with Device Manager because:
 * 1. VGA text mode is available immediately at boot (BIOS sets it up)
 * 2. It's used BEFORE device_manager_init() is called
 * 3. It's a simple early-boot utility, not a real device driver
 */

#include "vga.h"

/* ============================================
 * VGA Text Mode Constants
 * ============================================ */
#define VGA_ADDR        0xB8000
#define VGA_WIDTH       80
#define VGA_HEIGHT      25
#define VGA_DEFAULT_ATTR 0x07  /* Light gray on black */

/* ============================================
 * State
 * ============================================ */
static volatile unsigned short *vga = (unsigned short*)VGA_ADDR;
static int vga_x = 0;
static int vga_y = 0;
static unsigned char vga_current_attr = VGA_DEFAULT_ATTR;

/* ============================================
 * Functions
 * ============================================ */

void vga_clear(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga[i] = (vga_current_attr << 8) | ' ';
    }
    vga_x = 0;
    vga_y = 0;
}

void vga_set_color(unsigned char fg, unsigned char bg) {
    vga_current_attr = (bg << 4) | (fg & 0x0F);
}

void vga_draw_rect(int x, int y, int width, int height, unsigned char color) {
    unsigned char attr = (color << 4) | color;
    
    for (int row = y; row < y + height && row < VGA_HEIGHT; row++) {
        for (int col = x; col < x + width && col < VGA_WIDTH; col++) {
            int pos = row * VGA_WIDTH + col;
            vga[pos] = (attr << 8) | 0xDB;  /* Filled block character */
        }
    }
}

void vga_print(const char *s) {
    while (*s) {
        if (*s == '\n') {
            vga_x = 0;
            vga_y++;
            if (vga_y >= VGA_HEIGHT) vga_y = VGA_HEIGHT - 1;
            s++;
            continue;
        }
        
        if (vga_x >= VGA_WIDTH) {
            vga_x = 0;
            vga_y++;
            if (vga_y >= VGA_HEIGHT) vga_y = VGA_HEIGHT - 1;
        }
        
        int pos = vga_y * VGA_WIDTH + vga_x;
        vga[pos] = (vga_current_attr << 8) | (unsigned char)*s;
        vga_x++;
        s++;
    }
}

void vga_putchar(char c) {
    if (c == '\n') {
        vga_x = 0;
        vga_y++;
        if (vga_y >= VGA_HEIGHT) vga_y = VGA_HEIGHT - 1;
        return;
    }
    
    if (vga_x >= VGA_WIDTH) {
        vga_x = 0;
        vga_y++;
        if (vga_y >= VGA_HEIGHT) vga_y = VGA_HEIGHT - 1;
    }
    
    int pos = vga_y * VGA_WIDTH + vga_x;
    vga[pos] = (vga_current_attr << 8) | (unsigned char)c;
    vga_x++;
}

void vga_putint(int num) {
    char buffer[32];
    int count = 0;
    
    if (num == 0) {
        vga_putchar('0');
        return;
    }
    
    int n = (num < 0) ? -num : num;
    
    while (n > 0) {
        buffer[count++] = '0' + (n % 10);
        n = n / 10;
    }
    
    if (num < 0) {
        vga_putchar('-');
    }
    
    for (int i = count - 1; i >= 0; i--) {
        vga_putchar(buffer[i]);
    }
}

void vga_print_at(int x, int y, const char *s) {
    vga_x = x;
    vga_y = y;
    vga_print(s);
}
