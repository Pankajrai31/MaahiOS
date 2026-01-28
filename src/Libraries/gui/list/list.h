/**
 * MaahiOS - List Control API
 */

#ifndef MAAHI_LIST_H
#define MAAHI_LIST_H

/**
 * Create a list control
 * @param window_id Parent window
 * @param x, y      Position relative to window
 * @param width     List width
 * @param height    List height
 * @param items     Newline-separated list items
 * @return Control ID (>0) or -1 on error
 */
int maahi_create_list(int window_id, int x, int y, int width, int height, const char *items);

#endif /* MAAHI_LIST_H */
