/**
 * MaahiOS Window Library - Button Control Header (Design System v2)
 *
 * Description:
 *   Themed push button with styles matching the MaahiOS Design System v2:
 *   Standard, Default, Accent, Flat, Success, Danger.
 *
 *   All buttons use 3D embossed bevel borders. On press, bevel inverts
 *   to sunken and text shifts +1,+1 for tactile feedback.
 *
 *   Responds to mouse click events and calls the on_click callback.
 *
 * Author: MaahiOS Team
 * Date: March 2026
 */

#ifndef BUTTON_H
#define BUTTON_H

#include "control.h"
#include <stdint.h>

/*=============================================================================
 * BUTTON STYLES (Design System v2)
 *
 * BTN_STANDARD  — Chrome bg, raised bevel, dark text       (most common)
 * BTN_DEFAULT   — Standard + teal glow outline + bold text  (dialog OK)
 * BTN_ACCENT    — Blue gradient bg, blue bevel, white text  (primary CTA)
 * BTN_FLAT      — Transparent, hover reveals bevel          (toolbars)
 * BTN_SUCCESS   — Green bg, green bevel, white text
 * BTN_DANGER    — Red bg, red bevel, white text
 *===========================================================================*/

typedef enum {
    BTN_STANDARD = 0,   /* Chrome bg, raised bevel, dark text       */
    BTN_ACCENT,         /* Blue gradient, blue bevel, white text    */
    BTN_DEFAULT,        /* Standard + teal glow outline + bold      */
    BTN_FLAT,           /* Transparent, hover reveals bevel         */
    BTN_SUCCESS,        /* Green bg, green bevel, white text        */
    BTN_DANGER,         /* Red bg, red bevel, white text            */
} button_style_t;

/* Backward-compatibility aliases */
#define BTN_PRIMARY    BTN_STANDARD
#define BTN_SECONDARY  BTN_ACCENT
#define BTN_OUTLINE    BTN_DEFAULT
#define BTN_GHOST      BTN_FLAT

/*=============================================================================
 * BUTTON SIZES (Design System v2)
 *
 * BTN_SIZE_NORMAL — padding 5px 20px, min 75x25  (default)
 * BTN_SIZE_SMALL  — padding 2px 10px, min 50x20  (compact / toolbar)
 * BTN_SIZE_LARGE  — padding 7px 28px, min 100x30 (dialog / prominent)
 *===========================================================================*/

typedef enum {
    BTN_SIZE_NORMAL = 0,
    BTN_SIZE_SMALL,
    BTN_SIZE_LARGE,
} button_size_t;

/*=============================================================================
 * BUTTON STATES
 *===========================================================================*/

typedef enum {
    BTN_STATE_NORMAL = 0,
    BTN_STATE_HOVER,
    BTN_STATE_PRESSED,
    BTN_STATE_DISABLED,
} button_state_t;

/*=============================================================================
 * BUTTON STRUCT
 *===========================================================================*/

#define BUTTON_MAX_LABEL 32

typedef struct {
    control_t      base;            /* MUST be first member             */
    char           label[BUTTON_MAX_LABEL];
    button_style_t style;
    button_size_t  size;            /* BTN_SIZE_NORMAL / SMALL / LARGE  */
    button_state_t state;
    void         (*on_click)(void *userdata);
    void          *click_data;
} button_t;

/*=============================================================================
 * API
 *===========================================================================*/

/**
 * button_create - Create a new button control
 * @x:     X position relative to window content area
 * @y:     Y position relative to window content area
 * @w:     Width (0 = auto-size from label + padding)
 * @h:     Height (0 = THEME_BTN_HEIGHT)
 * @label: Button label text (copied)
 * @style: Visual style (BTN_PRIMARY, BTN_OUTLINE, etc.)
 * 
 * Returns: Pointer to new button, or NULL on failure.
 */
button_t *button_create(int x, int y, int w, int h,
                        const char *label, button_style_t style);

/**
 * button_set_label - Update button text
 * @btn:   Button to update
 * @label: New label (copied)
 */
void button_set_label(button_t *btn, const char *label);

/**
 * button_set_size - Set button size variant
 * @btn:  Button
 * @size: BTN_SIZE_NORMAL, BTN_SIZE_SMALL, or BTN_SIZE_LARGE
 *
 * Recalculates width/height from label + padding for the new size.
 */
void button_set_size(button_t *btn, button_size_t size);

/**
 * button_set_on_click - Set the click callback
 * @btn:      Button
 * @callback: Function called on click
 * @userdata: Passed to callback
 */
void button_set_on_click(button_t *btn,
                         void (*callback)(void *userdata),
                         void *userdata);

/**
 * button_destroy - Free button resources
 * @btn: Button to destroy
 */
void button_destroy(button_t *btn);

#endif /* BUTTON_H */
