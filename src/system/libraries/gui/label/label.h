/**
 * MaahiOS - Label API
 */

#ifndef MAAHI_LABEL_H
#define MAAHI_LABEL_H

/**
 * Create a text label
 * @param window_id Parent window
 * @param x, y      Position relative to window
 * @param text      Label text
 * @return Control ID (>0) or -1 on error
 */
int maahi_create_label(int window_id, int x, int y, const char *text);

/**
 * Update control text (labels, lists, etc.)
 * @param control_id Control to update
 * @param text      New text
 * @return 0 on success
 */
int maahi_update_text(int control_id, const char *text);

#endif /* MAAHI_LABEL_H */
