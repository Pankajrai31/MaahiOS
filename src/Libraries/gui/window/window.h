/**
 * MaahiOS - Window Management API
 */

#ifndef MAAHI_WINDOW_H
#define MAAHI_WINDOW_H

/**
 * Create a new window
 * @param x, y      Position on screen
 * @param width     Window width
 * @param height    Window height
 * @param title     Window title text
 * @return Window ID (>0) or -1 on error
 */
int maahi_create_window(int x, int y, int width, int height, const char *title);

/**
 * Set window icon
 * @param window_id Window to set icon for
 * @param icon_name Icon name (e.g., "folder_1")
 */
void maahi_set_window_icon(int window_id, const char *icon_name);

#endif /* MAAHI_WINDOW_H */
