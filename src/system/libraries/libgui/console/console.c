/**
 * MaahiOS GUI Library - Console/Terminal Widget Implementation
 * 
 * Description:
 *   Text console abstraction built on top of libgui drawing primitives.
 *   Manages cursor position, line wrapping, scrolling, and text output
 *   within a windowed rectangular area.
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#include "console.h"
#include "../printgui/printgui.h"
#include "../fonts/font8x16.h"

/*=============================================================================
 * INITIALIZATION
 *===========================================================================*/

void gui_console_init(gui_console_t *con,
                      int x, int y, int w, int h, int padding,
                      uint32_t bg, uint32_t fg, uint32_t border) {
    con->win_x = x;
    con->win_y = y;
    con->win_w = w;
    con->win_h = h;
    con->padding = padding;

    con->text_x = x + padding;
    con->text_y = y + padding;
    con->text_w = w - 2 * padding;
    con->text_h = h - 2 * padding;

    con->cols = con->text_w / FONT_CHAR_WIDTH;
    con->rows = con->text_h / FONT_CHAR_HEIGHT;

    con->cursor_row = 0;
    con->cursor_col = 0;

    con->bg_color = bg;
    con->fg_color = fg;
    con->border_color = border;

    con->cursor_visible = 0;
}

void gui_console_draw_window(gui_console_t *con) {
    gui_create_window(con->win_x, con->win_y, con->win_w, con->win_h,
                      con->bg_color, con->border_color);
}

/*=============================================================================
 * TEXT OUTPUT
 *===========================================================================*/

void gui_console_clear(gui_console_t *con) {
    gui_fill_rect(con->text_x, con->text_y,
                  con->text_w, con->text_h, con->bg_color);
    con->cursor_row = 0;
    con->cursor_col = 0;
}

void gui_console_scroll_up(gui_console_t *con) {
    gui_scroll_rect_up(con->text_x, con->text_y,
                       con->text_w, con->text_h,
                       FONT_CHAR_HEIGHT, con->bg_color);
}

void gui_console_putchar(gui_console_t *con, char c) {
    if (c == '\n') {
        con->cursor_col = 0;
        con->cursor_row++;
        if (con->cursor_row >= con->rows) {
            gui_console_scroll_up(con);
            con->cursor_row = con->rows - 1;
        }
        return;
    }

    if (con->cursor_col >= con->cols) {
        con->cursor_col = 0;
        con->cursor_row++;
        if (con->cursor_row >= con->rows) {
            gui_console_scroll_up(con);
            con->cursor_row = con->rows - 1;
        }
    }

    int px = con->text_x + con->cursor_col * FONT_CHAR_WIDTH;
    int py = con->text_y + con->cursor_row * FONT_CHAR_HEIGHT;
    gui_draw_char(px, py, c, con->fg_color, con->bg_color);
    con->cursor_col++;
}

void gui_console_print(gui_console_t *con, const char *str) {
    while (*str) {
        gui_console_putchar(con, *str);
        str++;
    }
}

void gui_console_print_color(gui_console_t *con, const char *str,
                             uint32_t color) {
    /* Temporarily swap fg color for this print */
    uint32_t saved_fg = con->fg_color;
    con->fg_color = color;

    while (*str) {
        gui_console_putchar(con, *str);
        str++;
    }

    con->fg_color = saved_fg;
}

void gui_console_print_int(gui_console_t *con, int val) {
    if (val < 0) {
        gui_console_putchar(con, '-');
        val = -val;
    }
    if (val == 0) {
        gui_console_putchar(con, '0');
        return;
    }

    char digits[12];
    int i = 0;
    while (val > 0) {
        digits[i++] = '0' + (char)(val % 10);
        val /= 10;
    }
    while (i > 0) {
        gui_console_putchar(con, digits[--i]);
    }
}

/*=============================================================================
 * INPUT HELPERS
 *===========================================================================*/

void gui_console_backspace(gui_console_t *con) {
    /* Erase cursor first */
    gui_console_erase_cursor(con);

    /* Move cursor back */
    if (con->cursor_col > 0) {
        con->cursor_col--;
    } else if (con->cursor_row > 0) {
        con->cursor_row--;
        con->cursor_col = con->cols - 1;
    } else {
        return;   /* Already at (0,0) */
    }

    /* Clear the character cell */
    int px = con->text_x + con->cursor_col * FONT_CHAR_WIDTH;
    int py = con->text_y + con->cursor_row * FONT_CHAR_HEIGHT;
    gui_fill_rect(px, py, FONT_CHAR_WIDTH, FONT_CHAR_HEIGHT, con->bg_color);
}

/*=============================================================================
 * CURSOR OPERATIONS
 *===========================================================================*/

void gui_console_draw_cursor(gui_console_t *con) {
    int px = con->text_x + con->cursor_col * FONT_CHAR_WIDTH;
    int py = con->text_y + con->cursor_row * FONT_CHAR_HEIGHT;
    gui_draw_cursor(px, py, con->fg_color);
    con->cursor_visible = 1;
}

void gui_console_erase_cursor(gui_console_t *con) {
    int px = con->text_x + con->cursor_col * FONT_CHAR_WIDTH;
    int py = con->text_y + con->cursor_row * FONT_CHAR_HEIGHT;
    gui_erase_cursor(px, py, con->bg_color);
    con->cursor_visible = 0;
}
