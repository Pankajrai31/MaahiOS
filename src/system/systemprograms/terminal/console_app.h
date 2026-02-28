/**
 * MaahiOS Console App Framework
 * 
 * Description:
 *   Defines the interface for console applications that run inside
 *   the Terminal. Each app provides init/handle_command/cleanup callbacks.
 *   Terminal manages the lifecycle: prompt switching, command routing, exit.
 * 
 * Usage:
 *   1. Implement init, handle_command, cleanup functions
 *   2. Define a console_app_t struct with name, description, and callbacks
 *   3. Register in terminal.c's app registry array
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#ifndef CONSOLE_APP_H
#define CONSOLE_APP_H

#include "../../libraries/libgui/libgui.h"

/*=============================================================================
 * CONSOLE APP INTERFACE
 *===========================================================================*/

typedef struct {
    const char *name;           /* App name (shown as prompt prefix) */
    const char *description;    /* One-line description for help listing */
    
    /**
     * Called when user launches the app.
     * Print welcome message, show initial help, etc.
     */
    void (*init)(gui_console_t *con);
    
    /**
     * Called for each command the user types while app is active.
     * The "exit" command is handled by terminal, not forwarded here.
     */
    void (*handle_command)(gui_console_t *con, const char *cmd);
    
    /**
     * Called when app is closing (user typed "exit").
     * Clean up any app-specific state.
     */
    void (*cleanup)(gui_console_t *con);
} console_app_t;

/*=============================================================================
 * COLORS (shared by all apps for consistency)
 *===========================================================================*/

#define APP_COLOR_HEADING       0x0000AAFF   /* Blue headings */
#define APP_COLOR_SUCCESS       0x0000CC00   /* Green success */
#define APP_COLOR_ERROR         0x00FF4444   /* Red errors */
#define APP_COLOR_WARN          0x00FFCC00   /* Yellow warnings */
#define APP_COLOR_INFO          0x00CCCCCC   /* Light gray info */
#define APP_COLOR_HIGHLIGHT     0x00FFFFFF   /* White highlights */

/*=============================================================================
 * APP STRING HELPERS (shared by all apps)
 *===========================================================================*/

static inline int app_str_equal(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++; b++;
    }
    return (*a == *b);
}

static inline int app_str_starts_with(const char *str, const char *prefix) {
    while (*prefix) {
        if (*str != *prefix) return 0;
        str++; prefix++;
    }
    return 1;
}

static inline int app_str_to_int(const char *s) {
    int result = 0;
    int negative = 0;
    
    while (*s == ' ') s++;
    if (*s == '-') { negative = 1; s++; }
    
    while (*s >= '0' && *s <= '9') {
        result = result * 10 + (*s - '0');
        s++;
    }
    
    return negative ? -result : result;
}

static inline const char *app_skip_spaces(const char *s) {
    while (*s == ' ') s++;
    return s;
}

#endif /* CONSOLE_APP_H */
