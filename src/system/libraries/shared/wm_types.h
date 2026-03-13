/**
 * MaahiOS Window Manager — Shared Types
 *
 * Description:
 *   Data structures shared between the WM Executive, libwm (client
 *   library), libwindow, and Orbit.  Fits within cell data limits.
 *
 * Author: MaahiOS Team
 * Date: March 2026
 */

#ifndef WM_TYPES_H
#define WM_TYPES_H

#include <stdint.h>

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define WM_MAX_WINDOWS      6       /* Max simultaneous windows              */
#define WM_TITLE_MAX        16      /* Max title chars (including NUL)       */

/* Flag bit for exec_request_t.flags — tells executive to skip response push */
#define EXEC_FLAG_NO_RESPONSE  0x01

/*=============================================================================
 * WM COMMAND IDS  (func_id values in exec_request_t)
 *===========================================================================*/

#define WM_CMD_CREATE       1       /* Register a new window                 */
#define WM_CMD_DESTROY      2       /* Unregister (close)                    */
#define WM_CMD_MOVE         3       /* Reposition window                     */
#define WM_CMD_DAMAGE       4       /* Repaint a dirty rectangle             */
#define WM_CMD_MINIMIZE     5       /* Minimize (hide)                       */
#define WM_CMD_RESTORE      6       /* Restore from minimized                */
#define WM_CMD_MAXIMIZE     7       /* Maximize / toggle                     */
#define WM_CMD_RAISE        8       /* Bring to front                        */
#define WM_CMD_SET_TITLE    9       /* Update title bar text                 */
#define WM_CMD_FULL_DAMAGE  10      /* Repaint entire screen                 */
#define WM_CMD_GET_INPUT    11      /* Poll for input events                 */
#define WM_CMD_HEARTBEAT    12      /* Window heartbeat (app is alive)       */

/*=============================================================================
 * WINDOW STATE
 *===========================================================================*/

#define WM_STATE_NORMAL     0
#define WM_STATE_MINIMIZED  1
#define WM_STATE_MAXIMIZED  2

/*=============================================================================
 * WM COMMAND PAYLOAD STRUCTURES  (packed into exec_request_t.payload)
 *===========================================================================*/

/** WM_CMD_CREATE payload */
typedef struct {
    int32_t  x, y, w, h;           /* Initial position / size               */
    char     title[WM_TITLE_MAX];  /* Window title                          */
} wm_create_payload_t;             /* 32 bytes                              */

/** WM_CMD_MOVE payload */
typedef struct {
    int32_t  handle;                /* Window handle (returned by CREATE)    */
    int32_t  new_x, new_y;
} wm_move_payload_t;

/** WM_CMD_DAMAGE payload (relative to window) */
typedef struct {
    int32_t  handle;
    int32_t  x, y, w, h;           /* Dirty rect in window-local coords    */
} wm_damage_payload_t;

/** WM_CMD_MINIMIZE / RESTORE / MAXIMIZE / RAISE / DESTROY payload */
typedef struct {
    int32_t  handle;
} wm_handle_payload_t;

/** WM_CMD_SET_TITLE payload */
typedef struct {
    int32_t  handle;
    char     title[WM_TITLE_MAX];
} wm_title_payload_t;

/*=============================================================================
 * WM RESPONSE PAYLOAD STRUCTURES (packed into exec_response_t.payload)
 *===========================================================================*/

/** Response to WM_CMD_CREATE: returns handle + SHM ID for surface */
typedef struct {
    int32_t  handle;               /* WM-assigned window handle             */
    int32_t  surface_shm_id;      /* SHM region for the pixel surface      */
} wm_create_response_t;

/** Response to WM_CMD_GET_INPUT */
#define WM_INPUT_NONE       0
#define WM_INPUT_MOUSE      1
#define WM_INPUT_KEY        2
#define WM_INPUT_FOCUS      3     /* Focus gained/lost notification         */

typedef struct {
    uint8_t  type;                 /* WM_INPUT_* constant                   */
    uint8_t  pad[3];
    /* Mouse fields */
    int32_t  mouse_x;             /* Screen coordinates                    */
    int32_t  mouse_y;
    uint8_t  mouse_buttons;       /* Button mask                           */
    uint8_t  mouse_pressed;       /* Rising edge (press)                   */
    uint8_t  mouse_released;      /* Falling edge (release)                */
    uint8_t  pad2;
    /* Key fields */
    int32_t  scancode;
    int32_t  ascii;
    /* Focus */
    uint8_t  focused;             /* 1 = gained, 0 = lost                  */
    uint8_t  pad3[3];
} wm_input_event_t;

/*=============================================================================
 * WINDOW REGISTRY  (published via cell by WM Executive, read by Orbit)
 *
 * Struct size: 4 + 6*32 = 196 bytes ≤ 256 cell max
 *===========================================================================*/

typedef struct {
    int32_t  pid;
    int32_t  handle;
    int16_t  x, y, w, h;
    uint8_t  state;                /* WM_STATE_*                            */
    uint8_t  z_order;              /* 0 = bottom                            */
    uint8_t  minimized;            /* For taskbar display                   */
    uint8_t  not_responding;       /* 1 = window is not responding          */
    char     title[WM_TITLE_MAX];
} wm_window_entry_t;               /* 32 bytes                              */

typedef struct {
    int32_t  count;
    wm_window_entry_t windows[WM_MAX_WINDOWS];
} wm_window_registry_t;            /* 4 + 6*32 = 196 bytes                  */

/*=============================================================================
 * CELL KEY NAMES
 *===========================================================================*/

#define CELL_WM_REQ_QUEUE       "system.wm.req_queue"
#define CELL_WM_RESP_QUEUE      "system.wm.resp_queue"
#define CELL_WM_REGISTRY        "system.wm.registry"
#define CELL_WM_READY           "system.wm.ready"

#endif /* WM_TYPES_H */
