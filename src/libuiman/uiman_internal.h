/**
 * UIMan Internal Structures
 * Shared between library and UIManager process
 */

#ifndef UIMAN_INTERNAL_H
#define UIMAN_INTERNAL_H

#include "uiman.h"

#define MAX_WINDOWS 32
#define MAX_CONTROLS 256
#define MAX_PROCESSES 64
#define EVENT_QUEUE_SIZE 32

// Window structure
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
} UIWindow;

// Control structure
typedef struct {
    int active;
    int id;
    int window_id;
    int owner_pid;
    int type;
    int x, y, width, height;  // Relative to window
    int state;
    char text[128];
    // Control-specific data
    union {
        struct { int rows, cols; } table;
        struct { int group; } radio;
    } data;
} UIControl;

// Event queue per process
typedef struct {
    uiman_event_t events[EVENT_QUEUE_SIZE];
    volatile int head;
    volatile int tail;
    volatile int count;
} EventQueue;

// Global registry (in uiman.c)
extern UIWindow g_windows[MAX_WINDOWS];
extern UIControl g_controls[MAX_CONTROLS];
extern EventQueue g_event_queues[MAX_PROCESSES];
extern volatile int g_next_window_id;
extern volatile int g_next_control_id;

// Internal functions
int uiman_find_free_window(void);
int uiman_find_free_control(void);
int uiman_hit_test(int x, int y);
void uiman_queue_event(int owner_pid, uiman_event_t *event);
void uiman_render_all(void);

#endif // UIMAN_INTERNAL_H
