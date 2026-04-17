/**
 * MaahiOS Window Library - Public API
 * 
 * Description:
 *   Main header for creating and managing GUI windows.
 *   An app creates a window, adds controls (buttons, labels, etc.),
 *   then calls window_run() to enter the event loop.
 * 
 *   Currently renders directly to the framebuffer via libgui.
 *   Will migrate to SHM surface + WM Executive compositing later.
 * 
 * Usage:
 *   #include "libwindow.h"
 * 
 *   window_t *win = window_create("My App", 200, 100, 400, 300);
 *   button_t *btn = button_create(10, 10, 0, 0, "OK", BTN_PRIMARY);
 *   window_add_control(win, &btn->base);
 *   button_set_on_click(btn, my_callback, NULL);
 *   window_run(win);
 *   window_destroy(win);
 * 
 * Author: MaahiOS Team
 * Date: March 2026
 */

#ifndef LIBWINDOW_H
#define LIBWINDOW_H

#include <stdint.h>
#include "surface.h"
#include "theme.h"
#include "controls/control.h"
#include "controls/label.h"
#include "controls/button.h"
#include "controls/dialog.h"
#include "controls/table.h"
#include "controls/toolbar.h"
#include "controls/statusbar.h"
#include "controls/menubar.h"
#include "controls/treeview.h"
#include "controls/textarea.h"
#include "controls/radiogroup.h"
#include "controls/tabs.h"
#include "../shared/taskbar_types.h"

/*=============================================================================
 * WINDOW CONSTANTS
 *===========================================================================*/

#define WINDOW_MAX_CONTROLS  64     /* Max controls per window              */
#define WINDOW_MAX_TITLE     64     /* Max title string length              */

/*=============================================================================
 * WINDOW FLAGS
 *===========================================================================*/

typedef enum {
    WIN_FLAG_NONE       = 0x00,
    WIN_FLAG_NO_CLOSE   = 0x01,     /* Hide close button                   */
    WIN_FLAG_NO_MINIMIZE = 0x02,    /* Hide minimize button                */
    WIN_FLAG_NO_MAXIMIZE = 0x04,    /* Hide maximize button                */
    WIN_FLAG_NO_TITLEBAR = 0x08,    /* Borderless window (no chrome)       */
    WIN_FLAG_NO_RESIZE  = 0x10,     /* Fixed size                          */
} window_flags_t;

/*=============================================================================
 * WINDOW STRUCT
 *===========================================================================*/

/* Forward declaration for self-referential callback types */
typedef struct window_s window_t;

struct window_s {
    /* Position and size (outer bounds including titlebar) */
    int x;
    int y;
    int width;
    int height;

    /* Title */
    char title[WINDOW_MAX_TITLE];

    /* Flags */
    uint32_t flags;

    /* Surface — the pixel buffer for the entire window (titlebar + content) */
    surface_t surface;

    /* Content area offset (below titlebar) */
    int content_x;             /* Always 0 for now (no left border) */
    int content_y;             /* THEME_TITLEBAR_HEIGHT             */
    int content_w;             /* width                             */
    int content_h;             /* height - THEME_TITLEBAR_HEIGHT    */

    /* Controls */
    control_t *controls[WINDOW_MAX_CONTROLS];
    int control_count;

    /* State */
    int running;               /* 1 = event loop active             */
    int needs_redraw;          /* 1 = full repaint needed           */
    int focused_control;       /* Index of focused control, -1=none */
    int hover_control;         /* Index of hovered control, -1=none */

    /* Drag state (outline drag-to-move) */
    int dragging;              /* 1 = currently dragging window     */
    int drag_offset_x;        /* Mouse X offset from window origin */
    int drag_offset_y;        /* Mouse Y offset from window origin */
    int drag_ghost_x;         /* Current ghost outline X           */
    int drag_ghost_y;         /* Current ghost outline Y           */

    /* Maximize state */
    int maximized;             /* 1 = currently maximized           */
    int restore_x;            /* Pre-maximize position             */
    int restore_y;
    int restore_w;
    int restore_h;

    /* Minimize state */
    int minimized;             /* 1 = window is hidden (minimized)  */

    /* Titlebar button hover/press state:
     * 0=none, 1=close, 2=minimize, 3=maximize */
    int hover_titlebar_btn;
    int pressed_titlebar_btn;

    /* WM Executive handle and SHM surface ID */
    int wm_handle;            /* Handle from WM Executive           */
    int surface_shm_id;       /* SHM ID for the pixel surface       */

    /* Callbacks */
    void (*on_close)(void *userdata);
    void *close_data;

    /* Titlebar icon — 16x16 decoded pixel buffer (0x00RRGGBB)
     * If non-NULL, drawn in the titlebar before the title text.
     * Caller owns the buffer; must remain valid while window is alive. */
    uint32_t *icon_pixels;

    /* Custom content callbacks (for apps like Terminal that need
     * full control over the content area rendering and keyboard) */
    void (*on_key)(window_t *win, int scancode, char ascii, void *userdata);
    void *on_key_data;

    void (*on_paint)(window_t *win, surface_t *surf, void *userdata);
    void *on_paint_data;

    void (*on_tick)(window_t *win, void *userdata);
    void *on_tick_data;

    void (*on_mouse)(window_t *win, int content_x, int content_y,
                     int event, void *userdata);
    void *on_mouse_data;
};

/*=============================================================================
 * WINDOW LIFECYCLE
 *===========================================================================*/

/**
 * window_create - Create a new window
 * @title:  Window title (shown in titlebar)
 * @x:      Screen X position
 * @y:      Screen Y position
 * @width:  Total window width (including borders)
 * @height: Total window height (including titlebar)
 * 
 * Returns: Pointer to window, or NULL on failure.
 */
window_t *window_create(const char *title, int x, int y, int width, int height);

/**
 * window_destroy - Destroy window and free all resources
 * @win: Window to destroy (also destroys all added controls)
 */
void window_destroy(window_t *win);

/*=============================================================================
 * CONTROL MANAGEMENT
 *===========================================================================*/

/**
 * window_add_control - Add a control to the window
 * @win:  Window
 * @ctrl: Control to add (position is relative to content area)
 * 
 * Returns: 0 on success, -1 if max controls reached
 */
int window_add_control(window_t *win, control_t *ctrl);

/**
 * window_remove_control - Remove a control from the window
 * @win:  Window
 * @ctrl: Control to remove (NOT freed — caller must destroy it)
 * 
 * Returns: 0 on success, -1 if not found
 */
int window_remove_control(window_t *win, control_t *ctrl);

/*=============================================================================
 * DRAWING
 *===========================================================================*/

/**
 * window_draw - Render the entire window (titlebar + controls)
 * @win: Window to draw
 * 
 * Draws to the window's internal surface, then blits to the framebuffer.
 */
void window_draw(window_t *win);

/*=============================================================================
 * EVENT LOOP
 *===========================================================================*/

/**
 * window_run - Enter the window's main event loop
 * @win: Window to run
 * 
 * Draws the window, then polls for keyboard/mouse events and
 * dispatches them to controls. Returns when the window is closed.
 */
void window_run(window_t *win);

/**
 * window_close - Request the window to close (exits window_run)
 * @win: Window to close
 */
void window_close(window_t *win);

/*=============================================================================
 * CALLBACKS
 *===========================================================================*/

/**
 * window_set_on_close - Set the close callback
 * @win:      Window
 * @callback: Called when user clicks the close button
 * @userdata: Passed to callback
 */
void window_set_on_close(window_t *win,
                         void (*callback)(void *userdata),
                         void *userdata);

/**
 * window_invalidate - Mark the window for full redraw
 * @win: Window to invalidate
 */
void window_invalidate(window_t *win);

/**
 * window_set_icon - Set a 16x16 titlebar icon (downscaled from 32x32 BMP)
 * @win:  Window
 * @bmp_data:  Raw BMP file bytes (loaded via libfs_read_file)
 * @bmp_size:  Size of the BMP data in bytes
 *
 * Decodes the BMP, downscales from 32x32 to 16x16, and stores the
 * pixels internally. Pass NULL/0 to remove the icon.
 */
void window_set_icon(window_t *win, const uint8_t *bmp_data, int bmp_size);

/*=============================================================================
 * CUSTOM CONTENT CALLBACKS
 *
 * For apps like Terminal that need full control over content rendering
 * and keyboard input.  When set, these override the default behavior.
 *===========================================================================*/

/**
 * window_set_on_key - Set custom keyboard handler
 * @win:      Window
 * @callback: Called for every KEY_PRESSED event (overrides control dispatch)
 * @userdata: Passed to callback
 *
 * When set, keyboard events go to this callback instead of being
 * dispatched to focused controls.  ESC still closes the window.
 */
void window_set_on_key(window_t *win,
                       void (*callback)(window_t *win, int scancode,
                                        char ascii, void *userdata),
                       void *userdata);

/**
 * window_set_on_paint - Set custom content area painter
 * @win:      Window
 * @callback: Called during window_draw() to paint the content area.
 *            The callback should draw into surf at (win->content_x,
 *            win->content_y) with size (win->content_w, win->content_h).
 * @userdata: Passed to callback
 *
 * When set, the default draw_content() (white fill + controls) is
 * replaced by this callback.
 */
void window_set_on_paint(window_t *win,
                         void (*callback)(window_t *win, surface_t *surf,
                                          void *userdata),
                         void *userdata);

/**
 * window_set_on_tick - Set per-frame tick callback
 * @win:      Window
 * @callback: Called every iteration of window_run(), after redraw.
 *            Use for cursor blink, polling child processes, etc.
 * @userdata: Passed to callback
 */
void window_set_on_tick(window_t *win,
                        void (*callback)(window_t *win, void *userdata),
                        void *userdata);

/**
 * window_set_on_mouse - Set custom mouse handler for content area
 * @win:      Window
 * @callback: Called for mouse events in the content area.
 *            content_x/y are relative to content origin.
 *            event: 0=MOUSE_DOWN, 1=MOUSE_UP, 2=MOUSE_MOVE
 * @userdata: Passed to callback
 *
 * When set, mouse down/up/move in the content area goes to this
 * callback instead of being dispatched to controls.
 */
#define WIN_MOUSE_DOWN  0
#define WIN_MOUSE_UP    1
#define WIN_MOUSE_MOVE  2
void window_set_on_mouse(window_t *win,
                         void (*callback)(window_t *win, int content_x,
                                          int content_y, int event,
                                          void *userdata),
                         void *userdata);

#endif /* LIBWINDOW_H */
