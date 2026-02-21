#ifndef WINDOW_RENDER_H
#define WINDOW_RENDER_H

#include <stdint.h>

// Forward declarations
typedef struct UIWindow UIWindow;
typedef struct UIControl UIControl;

/**
 * Window Rendering Module
 * Handles drawing windows, title bars, and window decorations
 */

void draw_window(UIWindow* win, uint32_t* back_buffer, int screen_width, int screen_height);
void draw_window_title_bar(UIWindow* win, uint32_t* back_buffer, int screen_width, int screen_height);
void render_taskbar(uint32_t* back_buffer, int screen_width, int screen_height, UIWindow* windows, int max_windows);

#endif // WINDOW_RENDER_H
