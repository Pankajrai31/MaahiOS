/**
 * MaahiOS Window Manager Client Library — Header
 *
 * Description:
 *   Thin client library for communicating with the WM Executive.
 *   Used by libwindow to register/unregister windows and request
 *   compositing.
 *
 * Author: MaahiOS Team
 * Date: March 2026
 */

#ifndef LIBWM_H
#define LIBWM_H

#include <stdint.h>
#include "../shared/wm_types.h"

/*=============================================================================
 * INITIALIZATION
 *===========================================================================*/

/**
 * libwm_init - Connect to the WM Executive
 *
 * Discovers WM SHM queue IDs via cells and attaches.
 * Auto-called by other libwm functions.
 *
 * Returns: 0 on success, -1 if WM Executive not ready
 */
int libwm_init(void);

/*=============================================================================
 * WINDOW LIFECYCLE
 *===========================================================================*/

/**
 * libwm_create - Register a new window with the WM
 * @x, @y, @w, @h: Initial screen position and size
 * @title:          Window title
 * @out_shm_id:     [out] SHM ID for the surface pixel buffer
 *
 * The WM allocates a SHM region for the surface.  The caller
 * attaches to this SHM and draws into it.
 *
 * Returns: Window handle (>0) on success, <0 on error
 */
int libwm_create(int x, int y, int w, int h, const char *title,
                 int *out_shm_id);

/**
 * libwm_destroy - Unregister and close a window
 * @handle: Window handle from libwm_create
 */
void libwm_destroy(int handle);

/*=============================================================================
 * WINDOW STATE
 *===========================================================================*/

/**
 * libwm_move - Move a window to a new position
 * @handle: Window handle
 * @new_x, @new_y: New screen position
 */
void libwm_move(int handle, int new_x, int new_y);

/**
 * libwm_damage - Signal that a region needs recompositing
 * @handle:        Window handle
 * @x, @y, @w, @h: Dirty rectangle (window-local coordinates)
 *
 * Call this after drawing to the surface to make changes visible.
 */
void libwm_damage(int handle, int x, int y, int w, int h);

/**
 * libwm_damage_full - Signal that the entire window needs recompositing
 * @handle: Window handle
 */
void libwm_damage_full(int handle);

/**
 * libwm_minimize - Minimize a window
 * @handle: Window handle
 */
void libwm_minimize(int handle);

/**
 * libwm_restore - Restore a window from minimized state
 * @handle: Window handle
 */
void libwm_restore(int handle);

/**
 * libwm_maximize - Toggle maximize on a window
 * @handle:      Window handle
 * @out_shm_id:  [out] New SHM ID if surface was reallocated
 *
 * Returns: 0 on success, <0 on error.
 *          Caller must re-attach SHM if out_shm_id changed.
 */
int libwm_maximize(int handle, int *out_shm_id);

/**
 * libwm_raise - Bring a window to the front
 * @handle: Window handle
 */
void libwm_raise(int handle);

/**
 * libwm_set_title - Update window title
 * @handle: Window handle
 * @title:  New title string
 */
void libwm_set_title(int handle, const char *title);

/**
 * libwm_full_composite - Request full screen recomposite
 */
void libwm_full_composite(void);

/**
 * libwm_is_focused - Check if a window currently has focus
 * @handle: Window handle
 *
 * Reads the WM registry cell to check focus state.
 * Returns: 1 if focused (topmost z_order), 0 otherwise
 */
int libwm_is_focused(int handle);

/**
 * libwm_heartbeat - Send a heartbeat to the WM Executive
 * @handle: Window handle
 *
 * Called by libwindow's event loop every iteration to signal
 * that the window's app is still alive and processing events.
 * The WM uses this to detect and flag "Not Responding" windows.
 */
void libwm_heartbeat(int handle);

#endif /* LIBWM_H */
