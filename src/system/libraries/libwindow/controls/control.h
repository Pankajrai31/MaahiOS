/**
 * MaahiOS Window Library - Base Control
 * 
 * Description:
 *   Every UI control (button, label, textbox, etc.) embeds this
 *   struct as its FIRST member, enabling polymorphic dispatch.
 * 
 *   The window maintains a flat list of control_t* pointers.
 *   Each control's draw/event functions are called via function
 *   pointers in the control_ops_t vtable.
 * 
 * Usage (for control implementors):
 *   typedef struct {
 *       control_t base;        // MUST be first member
 *       char label[32];        // control-specific fields
 *       int style;
 *   } button_t;
 * 
 * Author: MaahiOS Team
 * Date: March 2026
 */

#ifndef CONTROL_H
#define CONTROL_H

#include <stdint.h>

/* Forward declarations */
struct control;
struct surface;
typedef struct surface surface_t;

/*=============================================================================
 * EVENT TYPES
 *===========================================================================*/

typedef enum {
    GUI_EVENT_NONE = 0,
    GUI_EVENT_MOUSE_DOWN,
    GUI_EVENT_MOUSE_UP,
    GUI_EVENT_MOUSE_MOVE,
    GUI_EVENT_MOUSE_ENTER,
    GUI_EVENT_MOUSE_LEAVE,
    GUI_EVENT_KEY_DOWN,
    GUI_EVENT_KEY_UP,
    GUI_EVENT_FOCUS,
    GUI_EVENT_BLUR,
} gui_event_type_t;

typedef struct {
    gui_event_type_t type;
    int mouse_x;            /* Relative to window content area */
    int mouse_y;
    int mouse_button;       /* 0=left, 1=right, 2=middle      */
    uint8_t key_code;       /* Scancode for key events         */
    char key_char;          /* ASCII character (if printable)   */
} gui_event_t;

/*=============================================================================
 * CONTROL TYPES
 *===========================================================================*/

typedef enum {
    CONTROL_LABEL = 0,
    CONTROL_BUTTON,
    CONTROL_TEXTBOX,
    CONTROL_CHECKBOX,
    CONTROL_RADIO,
    CONTROL_TOGGLE,
    CONTROL_PANEL,
    CONTROL_LISTVIEW,
    CONTROL_PROGRESS,
    CONTROL_TABS,
    CONTROL_DROPDOWN,
    CONTROL_TABLE,
    CONTROL_ALERT,
    CONTROL_MENUBAR,
    CONTROL_TREEVIEW,
    CONTROL_TEXTAREA,
    CONTROL_TYPE_COUNT,
} control_type_t;

/*=============================================================================
 * CONTROL OPS (vtable)
 *===========================================================================*/

/**
 * control_ops_t - Virtual function table for control rendering/events
 * 
 * @draw:    Render the control onto the given surface
 * @event:   Handle an input event. Returns 1 if consumed, 0 if not.
 * @destroy: Free control-specific resources (NOT the struct itself)
 */
typedef struct {
    void (*draw)(struct control *ctrl, surface_t *surf);
    int  (*event)(struct control *ctrl, gui_event_t *evt);
    void (*destroy)(struct control *ctrl);
} control_ops_t;

/*=============================================================================
 * BASE CONTROL STRUCT
 *===========================================================================*/

/**
 * control_t - Base struct embedded at start of every control
 * 
 * @type:     Control type enum
 * @x, y:     Position relative to parent window's content area
 * @width:    Control width in pixels
 * @height:   Control height in pixels
 * @visible:  1 = draw this control, 0 = skip
 * @enabled:  1 = responds to events, 0 = disabled (grayed out)
 * @focused:  1 = currently has keyboard focus
 * @dirty:    1 = needs redraw, cleared after draw()
 * @ops:      Pointer to the control's vtable
 * @userdata: Arbitrary pointer for application use
 */
typedef struct control {
    control_type_t type;
    int x;
    int y;
    int width;
    int height;
    uint8_t visible;
    uint8_t enabled;
    uint8_t focused;
    uint8_t dirty;
    const control_ops_t *ops;
    void *userdata;
} control_t;

/*=============================================================================
 * HELPER MACROS
 *===========================================================================*/

/** Initialize common fields of a control base */
#define CONTROL_INIT(ctrl, _type, _x, _y, _w, _h, _ops) do { \
    (ctrl)->type    = (_type);                                 \
    (ctrl)->x       = (_x);                                    \
    (ctrl)->y       = (_y);                                    \
    (ctrl)->width   = (_w);                                    \
    (ctrl)->height  = (_h);                                    \
    (ctrl)->visible = 1;                                       \
    (ctrl)->enabled = 1;                                       \
    (ctrl)->focused = 0;                                       \
    (ctrl)->dirty   = 1;                                       \
    (ctrl)->ops     = (_ops);                                  \
    (ctrl)->userdata = (void *)0;                              \
} while (0)

/** Test if point (px, py) falls inside the control's bounds */
static inline int control_hit_test(const control_t *ctrl, int px, int py) {
    return (px >= ctrl->x && px < ctrl->x + ctrl->width &&
            py >= ctrl->y && py < ctrl->y + ctrl->height);
}

#endif /* CONTROL_H */
