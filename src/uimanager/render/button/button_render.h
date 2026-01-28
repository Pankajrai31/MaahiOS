/**
 * UIManager - Themed Button Renderer
 * Based on maahi-os-design-system.html
 */

#ifndef UIMANAGER_BUTTON_RENDER_H
#define UIMANAGER_BUTTON_RENDER_H

#include <stdint.h>

// Theme colors (from design system)
#define THEME_PRIMARY_DARK   0x131A22
#define THEME_PRIMARY_BLUE   0x2B5BB5
#define THEME_ACCENT_TEAL    0x4DC7A5
#define THEME_LIGHT_BLUE     0x40E5BF
#define THEME_HIGHLIGHT      0xF7F9FC
#define THEME_SUCCESS        0x28a745
#define THEME_WARNING        0xffc107
#define THEME_DANGER         0xdc3545
#define THEME_SECONDARY_GRAY 0x6c757d

// Hover states
#define THEME_PRIMARY_HOVER  0x1e4a9a

/**
 * Render themed button to back buffer
 * @param back_buffer Back buffer pointer
 * @param screen_width Screen width
 * @param x Button x position
 * @param y Button y position
 * @param width Button width
 * @param height Button height
 * @param text Button text
 * @param state Button state (0=normal, 1=hover, 2=pressed)
 * @param type Button type (0=primary, 1=secondary, etc.)
 */
void render_themed_button(uint32_t *back_buffer, int screen_width,
                          int x, int y, int width, int height,
                          const char *text, int state, int type);

#endif // UIMANAGER_BUTTON_RENDER_H
