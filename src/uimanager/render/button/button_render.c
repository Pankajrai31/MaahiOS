/**
 * UIManager - Themed Button Renderer Implementation
 */

#include "button_render.h"

// Forward declare draw functions (defined in uimanager.c for now)
extern void draw_to_back_buffer(int x, int y, int w, int h, unsigned int color);
extern void draw_text_to_back_buffer(int x, int y, const char *text, unsigned int fg, unsigned int bg);

// Helper: Simple strlen implementation
static int strlen(const char *str) {
    int len = 0;
    while (str[len]) len++;
    return len;
}

// Helper: Draw rounded rectangle with proper corner rendering
static void draw_rounded_rect(uint32_t *back_buffer, int screen_width,
                              int x, int y, int width, int height, 
                              unsigned int color, int radius) {
    // Clamp radius to avoid overlap
    if (radius > width / 2) radius = width / 2;
    if (radius > height / 2) radius = height / 2;
    
    // Draw main body rectangle (full rectangle first)
    draw_to_back_buffer(x, y, width, height, color);
    
    // Draw rounded corners using precise circle algorithm
    // For each corner, we use distance formula from corner center
    int radius_sq = radius * radius;
    
    for (int cy = 0; cy < radius; cy++) {
        for (int cx = 0; cx < radius; cx++) {
            // Distance from corner center (using integer math)
            int dx = radius - cx;
            int dy = radius - cy;
            int dist_sq = dx * dx + dy * dy;
            
            // If pixel is outside the circle, clear it (set to background)
            if (dist_sq > radius_sq) {
                int px, py;
                
                // Top-left corner - clear pixels outside circle
                px = x + cx;
                py = y + cy;
                if (px >= 0 && px < screen_width && py >= 0 && py < 768) {
                    back_buffer[py * screen_width + px] = 0xFFFFFF;  // White background
                }
                
                // Top-right corner
                px = x + width - 1 - cx;
                py = y + cy;
                if (px >= 0 && px < screen_width && py >= 0 && py < 768) {
                    back_buffer[py * screen_width + px] = 0xFFFFFF;
                }
                
                // Bottom-left corner
                px = x + cx;
                py = y + height - 1 - cy;
                if (px >= 0 && px < screen_width && py >= 0 && py < 768) {
                    back_buffer[py * screen_width + px] = 0xFFFFFF;
                }
                
                // Bottom-right corner
                px = x + width - 1 - cx;
                py = y + height - 1 - cy;
                if (px >= 0 && px < screen_width && py >= 0 && py < 768) {
                    back_buffer[py * screen_width + px] = 0xFFFFFF;
                }
            }
        }
    }
}

void render_themed_button(uint32_t *back_buffer, int screen_width,
                          int x, int y, int width, int height,
                          const char *text, int state, int type) {
    unsigned int bg_color;
    unsigned int text_color = 0xFFFFFF;  // White text for most buttons
    
    // Select colors based on type and state
    switch (type) {
        case 0:  // Primary
            if (state == 1) {  // Hover
                bg_color = THEME_PRIMARY_HOVER;
            } else if (state == 2) {  // Pressed
                bg_color = THEME_PRIMARY_HOVER;
            } else {
                bg_color = THEME_PRIMARY_BLUE;
            }
            break;
            
        case 1:  // Secondary
            bg_color = THEME_SECONDARY_GRAY;
            break;
            
        case 4:  // Success
            bg_color = THEME_SUCCESS;
            break;
            
        case 5:  // Danger
            bg_color = THEME_DANGER;
            break;
            
        default:
            bg_color = THEME_PRIMARY_BLUE;
    }
    
    // Draw button background with rounded corners (6px radius in theme)
    draw_rounded_rect(back_buffer, screen_width, x, y, width, height, bg_color, 6);
    
    // Draw text centered (vertically and horizontally)
    // Text is 14px in theme, our font is 8px, so adjust positioning
    int text_len = strlen(text);
    int text_x = x + (width / 2) - (text_len * 8) / 2;
    int text_y = y + (height / 2) - 4;  // Center vertically (8px font height)
    
    draw_text_to_back_buffer(text_x, text_y, text, text_color, 0);
    
    // TODO: Add shadow effect for hover state
    // box-shadow: 0 4px 12px rgba(43, 91, 181, 0.4);
}
