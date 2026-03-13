/**
 * MaahiOS VGA Text Mode Driver
 * Early boot console output (works before any driver init)
 */

#ifndef VGA_H
#define VGA_H

/**
 * Clear screen
 */
void vga_clear(void);

/**
 * Set text colors
 * @param fg Foreground color (0-15)
 * @param bg Background color (0-15)
 */
void vga_set_color(unsigned char fg, unsigned char bg);

/**
 * Print string
 */
void vga_print(const char *s);

/**
 * Print single character
 */
void vga_putchar(char c);

/**
 * Print integer
 */
void vga_putint(int num);

/**
 * Draw colored rectangle (using block characters)
 */
void vga_draw_rect(int x, int y, int width, int height, unsigned char color);

/**
 * Print string at specific position
 * @param x Column (0-79)
 * @param y Row (0-24)
 * @param s String to print
 */
void vga_print_at(int x, int y, const char *s);

#endif /* VGA_H */
