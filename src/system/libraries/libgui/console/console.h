/**
 * MaahiOS GUI Library - Console/Terminal Widget Header
 * 
 * Description:
 *   Provides a text console abstraction on top of libgui drawing
 *   primitives. Manages a windowed text area with cursor tracking,
 *   scrolling, text output, and cursor blinking.
 * 
 *   Any user-mode application can create a gui_console_t, call
 *   gui_console_init(), and use the print/input functions without
 *   any raw framebuffer access.
 * 
 * Usage:
 *   gui_console_t con;
 *   gui_console_init(&con, 112, 84, 800, 600, 8,
 *                    0x00000000, 0x00CCCCCC, 0x00444444);
 *   gui_console_draw_window(&con);
 *   gui_console_print(&con, "Hello, world!\n");
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#ifndef GUI_CONSOLE_H
#define GUI_CONSOLE_H

#include <stdint.h>

/*=============================================================================
 * CONSOLE STRUCTURE
 *===========================================================================*/

typedef struct {
    /* Window geometry (outer border) */
    int win_x, win_y, win_w, win_h;

    /* Text area (inside padding) */
    int text_x, text_y, text_w, text_h;
    int padding;

    /* Text grid dimensions (computed from text area and font size) */
    int cols, rows;

    /* Cursor position in the text grid */
    int cursor_row, cursor_col;

    /* Colors */
    uint32_t bg_color;
    uint32_t fg_color;
    uint32_t border_color;

    /* Cursor blink state */
    int cursor_visible;
} gui_console_t;

/*=============================================================================
 * INITIALIZATION
 *===========================================================================*/

/**
 * gui_console_init - Initialize a console widget
 * @con: Console structure to initialize
 * @x: Window left edge (pixels)
 * @y: Window top edge (pixels)
 * @w: Window width (pixels)
 * @h: Window height (pixels)
 * @padding: Padding between window edge and text area (pixels)
 * @bg: Background color (0x00RRGGBB)
 * @fg: Foreground/text color (0x00RRGGBB)
 * @border: Border color (0x00RRGGBB)
 */
void gui_console_init(gui_console_t *con,
                      int x, int y, int w, int h, int padding,
                      uint32_t bg, uint32_t fg, uint32_t border);

/**
 * gui_console_draw_window - Draw the console window (border + background)
 * @con: Console
 */
void gui_console_draw_window(gui_console_t *con);

/*=============================================================================
 * TEXT OUTPUT
 *===========================================================================*/

/**
 * gui_console_clear - Clear the text area and reset cursor to top-left
 * @con: Console
 */
void gui_console_clear(gui_console_t *con);

/**
 * gui_console_scroll_up - Scroll text area up by one line
 * @con: Console
 */
void gui_console_scroll_up(gui_console_t *con);

/**
 * gui_console_putchar - Put a character at cursor position and advance
 * @con: Console
 * @c: ASCII character (handles '\n' for newline)
 */
void gui_console_putchar(gui_console_t *con, char c);

/**
 * gui_console_print - Print a null-terminated string
 * @con: Console
 * @str: String to print
 */
void gui_console_print(gui_console_t *con, const char *str);

/**
 * gui_console_print_color - Print a string in a specific color
 * @con: Console
 * @str: String to print
 * @color: Text color (0x00RRGGBB)
 */
void gui_console_print_color(gui_console_t *con, const char *str,
                             uint32_t color);

/**
 * gui_console_print_int - Print an integer value as decimal text
 * @con: Console
 * @val: Integer to print (handles negatives and zero)
 */
void gui_console_print_int(gui_console_t *con, int val);

/*=============================================================================
 * INPUT HELPERS
 *===========================================================================*/

/**
 * gui_console_backspace - Erase the last character (move cursor back, clear cell)
 * @con: Console
 *
 * Caller is responsible for tracking input buffer length.
 * Does nothing if cursor is at (0,0).
 */
void gui_console_backspace(gui_console_t *con);

/*=============================================================================
 * CURSOR OPERATIONS
 *===========================================================================*/

/**
 * gui_console_draw_cursor - Draw cursor block at current position
 * @con: Console
 */
void gui_console_draw_cursor(gui_console_t *con);

/**
 * gui_console_erase_cursor - Erase cursor at current position
 * @con: Console
 */
void gui_console_erase_cursor(gui_console_t *con);

#endif /* GUI_CONSOLE_H */
