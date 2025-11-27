#include "libgui.h"
#include <stddef.h>

/**
 * Button and control functions
 */

static GUI_Button button_pool[32];
static int button_count = 0;

GUI_Button* gui_create_button(int x, int y, int width, int height, const char *text) {
    if (button_count >= 32) return NULL;
    
    GUI_Button *btn = &button_pool[button_count++];
    btn->x = x;
    btn->y = y;
    btn->width = width;
    btn->height = height;
    btn->bg_color = GUI_COLOR_GRAY;
    btn->text_color = GUI_COLOR_BLACK;
    btn->pressed = 0;
    
    // Copy text
    int i;
    for (i = 0; i < 31 && text[i] != '\0'; i++) {
        btn->text[i] = text[i];
    }
    btn->text[i] = '\0';
    
    return btn;
}

void gui_draw_button(GUI_Button *btn) {
    if (!btn) return;
    
    // Draw button background
    gui_draw_filled_rect(btn->x, btn->y, btn->width, btn->height, btn->bg_color);
    
    // Draw 3D border effect
    if (!btn->pressed) {
        // Raised button - white top/left, dark bottom/right
        gui_draw_rect(btn->x, btn->y, btn->width, btn->height, 0xFFFFFFFF);
        gui_draw_rect(btn->x + 1, btn->y + 1, btn->width - 1, btn->height - 1, 0xFF000000);
    } else {
        // Pressed button - dark top/left, white bottom/right
        gui_draw_rect(btn->x, btn->y, btn->width, btn->height, 0xFF000000);
        gui_draw_rect(btn->x + 1, btn->y + 1, btn->width - 1, btn->height - 1, 0xFFFFFFFF);
    }
    
    // Draw button text (centered-ish, transparent background)
    int text_x = btn->x + 8;
    int text_y = btn->y + 8;
    gui_draw_text(text_x, text_y, btn->text, 0xFF000000, 0);
}

/**
 * Simple button with shadow
 * Modern dark theme with subtle highlights
 */
void gui_button(const char *text, int x, int y) {
    // Draw dark shadow (offset by 3 pixels for subtlety)
    gui_draw_filled_rect(x + 3, y + 3, 150, 40, 0x000510);  // Very dark blue shadow
    
    // Draw main button with gradient-like effect (multiple layers)
    gui_draw_filled_rect(x, y, 150, 40, 0x003060);  // Medium blue button base
    
    // Add highlight at top for 3D effect
    gui_draw_filled_rect(x, y, 150, 2, 0x0055AA);  // Bright blue highlight line
    gui_draw_filled_rect(x, y + 2, 150, 2, 0x004488);  // Mid highlight
    
    // Draw bright cyan text using syscall
    extern void syscall_print_at(int x, int y, const char *str, unsigned int fg, unsigned int bg);
    syscall_print_at(x + 8, y + 12, text, 0xFF00FFFF, 0);  // Bright cyan with alpha
}
