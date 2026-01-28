#ifndef WINDOWS_MGMT_H
#define WINDOWS_MGMT_H

#include <stdint.h>

/* Maximum limits for UI elements */
#define MAX_WINDOWS 32
#define MAX_CONTROLS 256
#define MAX_PROCESSES 64
#define EVENT_QUEUE_SIZE 32

/* Control types */
#define UIMAN_CONTROL_BUTTON  1
#define UIMAN_CONTROL_LABEL   2
#define UIMAN_CONTROL_TEXTBOX 3
#define UIMAN_CONTROL_TABLE   4
#define UIMAN_CONTROL_RADIO   5
#define UIMAN_CONTROL_ICON    6
#define UIMAN_CONTROL_PANEL   7  // Flat colored rectangle (no 3D effect)
#define UIMAN_CONTROL_LIST    8  // Scrollable list of items

/* Control states */
#define UIMAN_STATE_NORMAL    0
#define UIMAN_STATE_HOVER     1
#define UIMAN_STATE_PRESSED   2
#define UIMAN_STATE_SELECTED  3

/* Event types */
#define UIMAN_EVENT_NONE      0
#define UIMAN_EVENT_CLICK     1
#define UIMAN_EVENT_DBLCLICK  2
#define UIMAN_EVENT_HOVER     3

/* Window states */
#define WINDOW_STATE_NORMAL     0
#define WINDOW_STATE_MINIMIZED  1
#define WINDOW_STATE_MAXIMIZED  2
#define WINDOW_STATE_PENDING_CLOSE  3  // Marked for cleanup

/* Event structure */
typedef struct {
    int type;
    int control_id;
    int x, y;
    int data;  // Extra data (e.g., selected list item index)
} uiman_event_t;

/* Window structure */
typedef struct {
    int active;
    int id;
    int parent_id;
    int owner_pid;
    int x, y, width, height;
    int z_order;
    int visible;
    int focused;
    char title[64];
    char icon_name[32];  // Icon filename (e.g., "folder_32" for folder_32.bmp)
    int state;  // WINDOW_STATE_NORMAL, MINIMIZED, MAXIMIZED, PENDING_CLOSE
    int saved_x, saved_y, saved_width, saved_height;  // For restore from min/max
} UIWindow;

/* Control structure */
typedef struct {
    int active;
    int id;
    int window_id;
    int owner_pid;
    int type;
    int x, y, width, height;
    int state;
    int button_size;  // 0=small, 1=medium, 2=large (for buttons)
    char text[128];
} UIControl;

/* Event queue structure */
typedef struct {
    uiman_event_t events[EVENT_QUEUE_SIZE];
    volatile int head;
    volatile int tail;
    volatile int count;
} EventQueue;

/**
 * Window Management API
 */

/* Initialize window management system */
void windows_mgmt_init(void);

/* Create a new window */
int uiman_create_window_kernel(int x, int y, int w, int h, const char *title, int parent, int owner_pid);

/* Set window icon by name */
void uiman_set_window_icon_kernel(int window_id, const char *icon_name);

/* Create UI controls */
int uiman_create_button_kernel(int window_id, int x, int y, int w, int h, const char *text, int owner_pid);
int uiman_create_label_kernel(int window_id, int x, int y, const char *text, int owner_pid);
int uiman_create_icon_kernel(int window_id, int x, int y, const char *text, int owner_pid);
int uiman_create_panel_kernel(int window_id, int x, int y, int w, int h, int color_style, const char *text, int owner_pid);
int uiman_create_list_kernel(int window_id, int x, int y, int w, int h, const char *items, int owner_pid);

/* Update control properties */
int uiman_update_control_text_kernel(int control_id, const char *text);

/* Event handling */
int uiman_poll_event_kernel(void *event_ptr, int calling_pid);

/* Get access to kernel window/control arrays (for UIManager rendering) */
UIWindow* uiman_get_kernel_windows(void);
UIControl* uiman_get_kernel_controls(void);
EventQueue* uiman_get_kernel_event_queues(void);

/* Window management functions (for Orbit smart launching) */
int uiman_find_window_by_title(const char *title);  // Returns window_id or -1
int uiman_get_window_state(int window_id);          // Returns state or -1
int uiman_restore_window(int window_id);            // Returns 0 or -1
int uiman_focus_window(int window_id);              // Returns 0 or -1

#endif // WINDOWS_MGMT_H
