/**
 * MaahiOS - Event API
 */

#ifndef MAAHI_EVENT_H
#define MAAHI_EVENT_H

/* Event types */
#define MAAHI_EVENT_NONE        0
#define MAAHI_EVENT_CLICK       1
#define MAAHI_EVENT_DBLCLICK    2
#define MAAHI_EVENT_KEYPRESS    3
#define MAAHI_EVENT_CLOSE       4

/* Event structure - what apps receive */
typedef struct {
    int type;           /* MAAHI_EVENT_* */
    int control_id;     /* Which control was clicked */
    int x, y;           /* Mouse position */
    int key;            /* Key code for KEYPRESS */
    int data;           /* Extra data */
} MaahiEvent;

/**
 * Poll for an event (non-blocking)
 * @param event     Pointer to event structure to fill
 * @return 1 if event available, 0 if no event
 */
int maahi_poll_event(MaahiEvent *event);

/**
 * Yield CPU to other processes
 * Call this in your main loop
 */
void maahi_yield(void);

#endif /* MAAHI_EVENT_H */
