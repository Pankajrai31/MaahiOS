/**
 * MaahiOS Window Library - Dialog Control Header
 *
 * Description:
 *   Modal dialog overlay for displaying messages and prompting user
 *   choices. Drawn as a centered overlay on top of the parent window.
 *
 *   Dialog Types:
 *     DIALOG_INFO    — Message + close icon, no buttons
 *     DIALOG_CONFIRM — Message + OK/Cancel buttons
 *     DIALOG_CUSTOM  — Message + up to 3 user-defined buttons
 *
 *   Usage:
 *     dialog_t *dlg = dialog_create(DIALOG_CONFIRM,
 *         "Delete File", "Are you sure?", 300, 140);
 *     dialog_set_on_result(dlg, my_callback, NULL);
 *     window_show_dialog(win, dlg);
 *     // Dialog runs modally inside window_run() event loop
 *
 * Author: MaahiOS Team
 * Date: March 2026
 */

#ifndef DIALOG_H
#define DIALOG_H

#include "control.h"
#include "../theme.h"
#include <stdint.h>

/*=============================================================================
 * DIALOG TYPES
 *===========================================================================*/

typedef enum {
    DIALOG_INFO = 0,        /* Message + close icon, no action buttons */
    DIALOG_CONFIRM,         /* Message + OK + Cancel buttons           */
    DIALOG_CUSTOM,          /* Message + user-defined buttons          */
} dialog_type_t;

/*=============================================================================
 * DIALOG RESULTS
 *===========================================================================*/

typedef enum {
    DIALOG_RESULT_NONE = 0,   /* Dialog still open / dismissed by close */
    DIALOG_RESULT_OK,         /* OK / primary button clicked            */
    DIALOG_RESULT_CANCEL,     /* Cancel / secondary button clicked      */
    DIALOG_RESULT_BTN1,       /* Custom button 1                        */
    DIALOG_RESULT_BTN2,       /* Custom button 2                        */
    DIALOG_RESULT_BTN3,       /* Custom button 3                        */
    DIALOG_RESULT_CLOSED,     /* Close icon clicked (DIALOG_INFO)       */
} dialog_result_t;

/*=============================================================================
 * DIALOG COLORS (Design System V2 themed — matches theme.h)
 *===========================================================================*/

/* Use theme.h constants directly since dialog.c includes theme.h */
#define DIALOG_BG           THEME_CHROME
#define DIALOG_SURFACE      THEME_SURFACE
#define DIALOG_SHADOW       0x00505060
#define DIALOG_TITLE_FG     THEME_TEXT_INVERSE
#define DIALOG_TEXT_FG      THEME_TEXT
#define DIALOG_TEXT_SEC     THEME_TEXT_SECONDARY
#define DIALOG_CLOSE_X_FG   THEME_TEXT

/*=============================================================================
 * DIALOG CONSTANTS
 *===========================================================================*/

#define DIALOG_MAX_TITLE    48
#define DIALOG_MAX_MESSAGE  128
#define DIALOG_MAX_BTN_LABEL 24
#define DIALOG_MAX_BUTTONS   3
#define DIALOG_TITLE_H      THEME_TITLEBAR_HEIGHT   /* 24px */
#define DIALOG_BTN_W        90
#define DIALOG_BTN_H        THEME_BTN_HEIGHT        /* 25px */
#define DIALOG_BTN_PAD      12
#define DIALOG_MARGIN       12
#define DIALOG_BEVEL_W       2

/*=============================================================================
 * DIALOG BUTTON DEFINITION
 *===========================================================================*/

typedef struct {
    char            label[DIALOG_MAX_BTN_LABEL];
    dialog_result_t result;    /* Result code returned on click */
    uint8_t         is_accent; /* 1 = red/accent style, 0 = standard */
} dialog_btn_def_t;

/*=============================================================================
 * DIALOG STRUCT
 *===========================================================================*/

typedef struct {
    control_t        base;             /* MUST be first member */
    dialog_type_t    type;
    char             title[DIALOG_MAX_TITLE];
    char             message[DIALOG_MAX_MESSAGE];

    /* Buttons */
    dialog_btn_def_t buttons[DIALOG_MAX_BUTTONS];
    int              button_count;

    /* State */
    int              active;           /* 1 = dialog is showing */
    dialog_result_t  result;           /* Set on dismiss */
    int              hover_btn;        /* -1=none, 0..2=button index */
    int              hover_close;      /* 1 = hovering close icon */

    /* Callback */
    void           (*on_result)(dialog_result_t result, void *userdata);
    void            *result_data;
} dialog_t;

/*=============================================================================
 * API
 *===========================================================================*/

/**
 * dialog_create - Create a new dialog control
 * @type:    Dialog type (DIALOG_INFO, DIALOG_CONFIRM, DIALOG_CUSTOM)
 * @title:   Dialog title bar text (copied)
 * @message: Message body text (copied)
 * @w:       Dialog width (0 = auto 300px)
 * @h:       Dialog height (0 = auto 140px)
 *
 * For DIALOG_CONFIRM, OK and Cancel buttons are auto-created.
 * For DIALOG_CUSTOM, use dialog_add_button() to add buttons.
 *
 * Returns: Pointer to new dialog, or NULL on failure.
 */
dialog_t *dialog_create(dialog_type_t type, const char *title,
                        const char *message, int w, int h);

/**
 * dialog_add_button - Add a custom button to a DIALOG_CUSTOM dialog
 * @dlg:       Dialog
 * @label:     Button label text (copied)
 * @result:    Result code returned when button is clicked
 * @is_accent: 1 = accent/danger style, 0 = standard style
 *
 * Returns: 0 on success, -1 if max buttons reached
 */
int dialog_add_button(dialog_t *dlg, const char *label,
                      dialog_result_t result, int is_accent);

/**
 * dialog_set_on_result - Set the result callback
 * @dlg:      Dialog
 * @callback: Called when dialog is dismissed with a result
 * @userdata: Passed to callback
 */
void dialog_set_on_result(dialog_t *dlg,
                          void (*callback)(dialog_result_t result,
                                           void *userdata),
                          void *userdata);

/**
 * dialog_show - Activate the dialog (makes it visible and modal)
 * @dlg: Dialog to show
 */
void dialog_show(dialog_t *dlg);

/**
 * dialog_dismiss - Dismiss the dialog with a result
 * @dlg:    Dialog to dismiss
 * @result: The result code
 */
void dialog_dismiss(dialog_t *dlg, dialog_result_t result);

/**
 * dialog_destroy - Free dialog resources
 * @dlg: Dialog to destroy (frees the struct)
 */
void dialog_destroy(dialog_t *dlg);

#endif /* DIALOG_H */
