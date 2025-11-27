#ifndef LIBGUI_H
#define LIBGUI_H

#include <stdint.h>

/* ============================================
 * MaahiOS LibGUI - User Space Graphics Library
 * Similar to Win32 API but simplified
 * ============================================ */

/* Color Constants */
#define GUI_COLOR_WHITE         0xFFFFFFFF
#define GUI_COLOR_BLACK         0xFF000000
#define GUI_COLOR_GRAY          0xFFC0C0C0
#define GUI_COLOR_DARK_GRAY     0xFF808080
#define GUI_COLOR_BLUE          0xFF0000FF
#define GUI_COLOR_RED           0xFFFF0000
#define GUI_COLOR_GREEN         0xFF00FF00
#define GUI_COLOR_YELLOW        0xFFFFFF00
#define GUI_COLOR_CYAN          0xFF00FFFF
#define GUI_COLOR_TEAL          0xFF008080
#define GUI_COLOR_NAVY          0xFF000080

/* Window Structure */
typedef struct {
    int x, y;
    int width, height;
    char title[64];
    uint32_t bg_color;
    uint32_t title_color;
    int visible;
} GUI_Window;

// Icons are now rendered via BMP files - no struct needed

/* Button Structure */
typedef struct {
    int x, y;
    int width, height;
    char text[32];
    uint32_t bg_color;
    uint32_t text_color;
    int pressed;
} GUI_Button;

// Desktop management removed - orbit draws directly

/* ============================================
 * Drawing Functions (draw.c)
 * ============================================ */
void gui_draw_filled_rect(int x, int y, int width, int height, uint32_t color);
void gui_draw_rect(int x, int y, int width, int height, uint32_t color);
void gui_draw_text(int x, int y, const char *text, uint32_t fg, uint32_t bg);
void gui_clear_screen(uint32_t color);

/* ============================================
 * Window Functions (window.c)
 * ============================================ */
GUI_Window* gui_create_window(int x, int y, int width, int height, const char *title, uint32_t bg_color);
void gui_draw_window(GUI_Window *win);
void gui_draw_window_title_bar(GUI_Window *win);
void gui_free_window(GUI_Window *win);

/* ============================================
 * Button Functions (controls.c)
 * ============================================ */
GUI_Button* gui_create_button(int x, int y, int width, int height, const char *text);
void gui_draw_button(GUI_Button *btn);

/**
 * Simple button with shadow - just text, no complex structures
 * Draws grey button on blue shadow
 */
void gui_button(const char *text, int x, int y);

#endif // LIBGUI_H
