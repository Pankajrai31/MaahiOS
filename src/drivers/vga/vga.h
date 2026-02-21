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

#endif /* VGA_H */
