/**
 * MaahiOS Taskbar & Desktop Shared Types
 *
 * Description:
 *   Data structures stored in cells for communication between:
 *   - libwindow  (registers/unregisters windows, sets minimized flag)
 *   - orbit      (reads taskbar entries, renders buttons, signals restore)
 *   - sysman     (publishes desktop app shortcuts)
 *
 *   All structures fit within CELL_DATA_MAX (256 bytes).
 *
 * Cell keys:
 *   "system.taskbar.windows" — list of running windowed apps
 *   "system.taskbar.restore" — PID of window to restore (written by orbit)
 *   "system.desktop.apps"    — list of desktop app shortcuts
 *
 * Author: MaahiOS Team
 * Date: March 2026
 */

#ifndef TASKBAR_TYPES_H
#define TASKBAR_TYPES_H

#include <stdint.h>

/*=============================================================================
 * CELL KEY NAMES
 *===========================================================================*/

#define CELL_TASKBAR_WINDOWS    "system.taskbar.windows"
#define CELL_TASKBAR_RESTORE    "system.taskbar.restore"
#define CELL_DESKTOP_APPS       "system.desktop.apps"

/*=============================================================================
 * TASKBAR WINDOW LIST  (written by libwindow, read by orbit)
 *
 * Each running window registers itself here.  Orbit polls this cell
 * to render dynamic taskbar buttons.
 *
 * Entry size: 28 bytes.  Max 9 entries → 9*28 + 4 = 256 bytes.
 *===========================================================================*/

#define TASKBAR_TITLE_MAX       20
#define TASKBAR_MAX_WINDOWS     6      /* 4 + 6*28 = 172 ≤ 188 safe payload */

typedef struct {
    int32_t  pid;                           /* Process ID                   */
    char     title[TASKBAR_TITLE_MAX];      /* Window title (truncated)     */
    uint8_t  minimized;                     /* 1 = window is minimized      */
    uint8_t  reserved[3];                   /* Padding to 28 bytes          */
} taskbar_entry_t;                          /* 28 bytes                     */

typedef struct {
    int32_t  count;                         /* Number of valid entries      */
    taskbar_entry_t entries[TASKBAR_MAX_WINDOWS]; /* 6 * 28 = 168           */
} taskbar_window_list_t;                    /* 172 bytes total              */

/*=============================================================================
 * TASKBAR RESTORE SIGNAL  (written by orbit, read by libwindow)
 *
 * When orbit wants a minimized window to restore, it writes the PID
 * here.  The window's event loop polls this cell and restores when
 * it sees its own PID.  After restoring, the window writes 0 to clear.
 *===========================================================================*/

typedef struct {
    int32_t  pid;                           /* PID to restore, 0 = none     */
} taskbar_restore_t;                        /* 4 bytes                      */

/*=============================================================================
 * DESKTOP APP SHORTCUTS  (written by sysman, read by orbit)
 *
 * Static list of apps available as clickable desktop shortcuts.
 * Entry size: 56 bytes.  Max 4 entries → 4*56 + 4 = 228 bytes.
 *===========================================================================*/

#define DESKTOP_APP_NAME_MAX    16
#define DESKTOP_APP_CMD_MAX     16
#define DESKTOP_ICON_NAME_MAX   16
#define DESKTOP_MAX_APPS        4      /* 4 + 4*56 = 228 ≤ 256 cell max   */

#define DESKTOP_APP_TYPE_MODULE 0           /* GRUB module (module_idx)     */
#define DESKTOP_APP_TYPE_MEX    1           /* .MEX file on filesystem      */

typedef struct {
    char     name[DESKTOP_APP_NAME_MAX];    /* Display name                 */
    char     command[DESKTOP_APP_CMD_MAX];  /* MEX filename (type=MEX only) */
    char     icon_file[DESKTOP_ICON_NAME_MAX]; /* Icon BMP filename on ISO  */
    uint8_t  app_type;                      /* DESKTOP_APP_TYPE_*           */
    uint8_t  module_idx;                    /* GRUB module index (type=MODULE) */
    uint8_t  is_gui;                        /* 1 = GUI app (fire-and-forget)*/
    uint8_t  _pad1;                         /* Alignment padding            */
    uint32_t bss_size;                      /* BSS size for module apps     */
} desktop_app_entry_t;                      /* 56 bytes                     */

typedef struct {
    int32_t  count;                         /* Number of valid entries      */
    desktop_app_entry_t entries[DESKTOP_MAX_APPS]; /* 4 * 56 = 224          */
} desktop_app_list_t;                       /* 228 bytes (fits 256)         */

#endif /* TASKBAR_TYPES_H */
