#include "icons.h"
#include "libgui.h"

/**
 * Icon Implementation - Nice looking colored squares with borders
 */

void icon_draw(IconType type, int x, int y) {
    uint32_t fill_color;
    
    // Select color based on icon type
    switch (type) {
        case ICON_PROCESS:
            fill_color = 0x0000FF;  // Blue
            break;
        case ICON_DISK:
            fill_color = 0xFF0000;  // Red
            break;
        case ICON_FILES:
            fill_color = 0xFFFF00;  // Yellow
            break;
        case ICON_NOTEBOOK:
            fill_color = 0x00FF00;  // Green
            break;
        default:
            fill_color = 0x808080;  // Grey fallback
            break;
    }
    
    // Draw icon as 48x48 square with nice borders
    // Outer white border (2px)
    gui_draw_filled_rect(x, y, 48, 48, 0xFFFFFF);
    // Dark grey inner border (1px)
    gui_draw_filled_rect(x + 2, y + 2, 44, 44, 0x404040);
    // Colored fill
    gui_draw_filled_rect(x + 3, y + 3, 42, 42, fill_color);
}

void icon_draw_with_label(IconType type, int x, int y, const char *label) {
    // Draw the icon
    icon_draw(type, x, y);
    
    // Draw label below icon (white text on transparent background)
    gui_draw_text(x, y + 52, label, 0xFFFFFF, 0);
}
