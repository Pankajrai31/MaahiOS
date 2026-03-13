/**
 * MaahiOS Window Library - TextArea Control Header (Design System v2)
 *
 * Description:
 *   Multi-line text editing control using the 8×16 bitmap font.
 *   Supports cursor movement, text insertion/deletion, scrolling.
 *   Provides a flat text buffer for direct read/write access.
 *
 *   Visual layout:
 *     ┌─── sunken border ─────────────────────────┐
 *     │ This is line 1                             │
 *     │ This is line 2_                            │  (_=cursor)
 *     │                                            │
 *     └────────────────────────────────────────────┘
 *
 *   Keyboard input comes via gui_event_t KEY_DOWN events.
 *   The host app must route keys to the textarea (e.g., via on_key).
 *
 * Author: MaahiOS Team
 * Date: March 2026
 */

#ifndef TEXTAREA_H
#define TEXTAREA_H

#include "control.h"
#include <stdint.h>

/*=============================================================================
 * TEXTAREA CONSTANTS
 *===========================================================================*/

#define TEXTAREA_MAX_TEXT    8192     /* Maximum text buffer size (bytes)   */
#define TEXTAREA_BORDER_W      2     /* Sunken border width                */
#define TEXTAREA_PAD_X         4     /* Left/right inner padding           */
#define TEXTAREA_PAD_Y         2     /* Top/bottom inner padding           */
#define TEXTAREA_CHAR_W        8     /* Font8x16 character width           */
#define TEXTAREA_CHAR_H       16     /* Font8x16 character height          */

/*=============================================================================
 * TEXTAREA STRUCT
 *===========================================================================*/

typedef struct {
    control_t   base;                /* MUST be first member               */

    /* Text buffer */
    char        text[TEXTAREA_MAX_TEXT];
    int         text_len;            /* Current text length (bytes)        */

    /* Cursor state */
    int         cursor_pos;          /* Byte offset in text                */
    int         cursor_line;         /* Computed: 0-based line number      */
    int         cursor_col;          /* Computed: 0-based column           */
    int         cursor_visible;      /* 1 = cursor drawn, 0 = blink off   */
    int         cursor_blink_tick;   /* Counter for blink toggling         */

    /* Scroll state */
    int         scroll_y;            /* First visible line (0-based)       */
    int         visible_lines;       /* Computed from height               */
    int         visible_cols;        /* Computed from width                */

    /* Edit state */
    int         modified;            /* 1 = text changed since last reset  */
    int         readonly;            /* 1 = no editing (view-only)         */
} textarea_t;

/*=============================================================================
 * API
 *===========================================================================*/

/**
 * textarea_create - Create a new textarea control
 * @x:  X position relative to window content area
 * @y:  Y position relative to window content area
 * @w:  Total width (including border)
 * @h:  Total height (including border)
 *
 * Returns: Pointer to new textarea, or NULL on failure.
 */
textarea_t *textarea_create(int x, int y, int w, int h);

/**
 * textarea_set_text - Set the textarea content (replaces all text)
 * @ta:   TextArea
 * @text: Null-terminated string to load
 *
 * Resets cursor to position 0 and clears modified flag.
 */
void textarea_set_text(textarea_t *ta, const char *text);

/**
 * textarea_get_text - Get pointer to the textarea's text buffer
 * @ta: TextArea
 *
 * Returns: Pointer to the internal null-terminated text buffer.
 *          Do NOT modify directly — use textarea_set_text or let
 *          the control handle editing via events.
 */
const char *textarea_get_text(textarea_t *ta);

/**
 * textarea_get_text_len - Get current text length
 * @ta: TextArea
 *
 * Returns: Number of bytes in the text buffer (excluding null terminator).
 */
int textarea_get_text_len(textarea_t *ta);

/**
 * textarea_is_modified - Check if text has been modified
 * @ta: TextArea
 *
 * Returns: 1 if modified since last set_text or clear_modified, 0 otherwise.
 */
int textarea_is_modified(textarea_t *ta);

/**
 * textarea_clear_modified - Clear the modified flag
 * @ta: TextArea
 */
void textarea_clear_modified(textarea_t *ta);

/**
 * textarea_set_readonly - Set read-only mode
 * @ta:       TextArea
 * @readonly: 1 = read-only, 0 = editable
 */
void textarea_set_readonly(textarea_t *ta, int readonly);

/**
 * textarea_handle_key - Process a key event for the textarea
 * @ta:       TextArea
 * @scancode: Keyboard scancode
 * @ascii:    ASCII character (0 if non-printable)
 *
 * Call this from the app's on_key handler.
 * Returns: 1 if the key was handled, 0 otherwise.
 */
int textarea_handle_key(textarea_t *ta, int scancode, char ascii);

/**
 * textarea_tick - Advance cursor blink timer
 * @ta: TextArea
 *
 * Call from on_tick. Toggles cursor visibility every ~25 ticks.
 */
void textarea_tick(textarea_t *ta);

/**
 * textarea_get_cursor_line - Get current cursor line (0-based)
 * @ta: TextArea
 */
int textarea_get_cursor_line(textarea_t *ta);

/**
 * textarea_get_cursor_col - Get current cursor column (0-based)
 * @ta: TextArea
 */
int textarea_get_cursor_col(textarea_t *ta);

/**
 * textarea_destroy - Free textarea resources
 * @ta: TextArea to destroy
 */
void textarea_destroy(textarea_t *ta);

#endif /* TEXTAREA_H */
