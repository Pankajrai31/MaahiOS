/**
 * MaahiOS - Icon API
 */

#ifndef MAAHI_ICON_H
#define MAAHI_ICON_H

/**
 * Create a desktop icon
 * @param window_id Parent window (0 for desktop)
 * @param x, y      Position
 * @param text      Icon label
 * @return Control ID (>0) or -1 on error
 */
int maahi_create_icon(int window_id, int x, int y, const char *text);

#endif /* MAAHI_ICON_H */
