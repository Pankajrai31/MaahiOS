/**
 * UIMan - UI Manager Client Library
 * Applications link against this to create windows and controls
 */

#ifndef UIMAN_H
#define UIMAN_H

// Event types
#define UIMAN_EVENT_NONE        0
#define UIMAN_EVENT_CLICK       1
#define UIMAN_EVENT_DBLCLICK    2
#define UIMAN_EVENT_HOVER       3
#define UIMAN_EVENT_KEY_PRESS   4
#define UIMAN_EVENT_PAINT       5

// Control types
#define UIMAN_CONTROL_WINDOW    1
#define UIMAN_CONTROL_BUTTON    2
#define UIMAN_CONTROL_LABEL     3
#define UIMAN_CONTROL_TEXTBOX   4
#define UIMAN_CONTROL_TABLE     5
#define UIMAN_CONTROL_RADIO     6

// Control states
#define UIMAN_STATE_NORMAL      0
#define UIMAN_STATE_HOVER       1
#define UIMAN_STATE_PRESSED     2
#define UIMAN_STATE_DISABLED    3

// Event structure
typedef struct {
    int type;          // CLICK, HOVER, KEY_PRESS, etc.
    int window_id;
    int control_id;
    int x, y;          // For mouse events
    int data;          // Button state, key code, etc.
} uiman_event_t;

// Window management API
int uiman_create_window(int x, int y, int w, int h, const char *title, int parent);
void uiman_close_window(int window_id);
void uiman_show_window(int window_id);
void uiman_hide_window(int window_id);

// Control creation API
int uiman_create_button(int window_id, int x, int y, int w, int h, const char *text);
int uiman_create_label(int window_id, int x, int y, const char *text);
int uiman_create_textbox(int window_id, int x, int y, int w, int h);
int uiman_create_table(int window_id, int x, int y, int w, int h, int rows, int cols);
int uiman_create_radio(int window_id, int x, int y, const char *text, int group);

// Event handling API
int uiman_get_event(uiman_event_t *event);   // Blocks until event available
int uiman_poll_event(uiman_event_t *event);  // Non-blocking, returns 0 if no event

// Control update API
void uiman_set_text(int control_id, const char *text);
void uiman_set_enabled(int control_id, int enabled);
void uiman_invalidate(int control_id);  // Request redraw

// Initialization
void uiman_init(void);

#endif // UIMAN_H
