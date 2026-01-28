/**
 * MaahiOS - Button API
 */

#ifndef MAAHI_BUTTON_H
#define MAAHI_BUTTON_H

/* Button types */
#define MAAHI_BUTTON_PRIMARY    0
#define MAAHI_BUTTON_SECONDARY  1
#define MAAHI_BUTTON_SUCCESS    2
#define MAAHI_BUTTON_DANGER     3

/* Button sizes */
#define MAAHI_BUTTON_SMALL      0
#define MAAHI_BUTTON_MEDIUM     1
#define MAAHI_BUTTON_LARGE      2

/**
 * Create a button
 * @param window_id Parent window
 * @param x, y      Position relative to window
 * @param width     Button width
 * @param height    Button height
 * @param text      Button label
 * @return Control ID (>0) or -1 on error
 */
int maahi_create_button(int window_id, int x, int y, int width, int height, const char *text);

/**
 * Create a themed button (future enhancement)
 * @param window_id Parent window
 * @param x, y      Position relative to window
 * @param width     Button width
 * @param height    Button height
 * @param text      Button label
 * @param type      Button type (PRIMARY, SECONDARY, etc.)
 * @param size      Button size (SMALL, MEDIUM, LARGE)
 * @return Control ID (>0) or -1 on error
 */
int maahi_button_create(int window_id, int x, int y, int width, int height,
                        const char *text, int type, int size);

#endif /* MAAHI_BUTTON_H */
