/**
 * MaahiOS Window Library - Label Control Header
 * 
 * Description:
 *   A static text label. Displays a string at a position
 *   using theme colors. Does not respond to input events.
 * 
 * Author: MaahiOS Team
 * Date: March 2026
 */

#ifndef LABEL_H
#define LABEL_H

#include "control.h"
#include <stdint.h>

/*=============================================================================
 * LABEL STRUCT
 *===========================================================================*/

#define LABEL_MAX_TEXT  128

typedef struct {
    control_t base;             /* MUST be first member         */
    char      text[LABEL_MAX_TEXT];
    uint32_t  fg_color;         /* Text color                   */
    uint32_t  bg_color;         /* Background (use parent bg for transparent) */
    int       transparent;      /* 1 = don't draw background    */
} label_t;

/*=============================================================================
 * API
 *===========================================================================*/

/**
 * label_create - Create a new label control
 * @x:    X position relative to window content area
 * @y:    Y position relative to window content area
 * @text: Display text (copied, max LABEL_MAX_TEXT-1 chars)
 * @fg:   Text color (0x00RRGGBB)
 * 
 * Returns: Pointer to new label, or NULL on failure.
 *          Width/height are auto-calculated from text length.
 */
label_t *label_create(int x, int y, const char *text, uint32_t fg);

/**
 * label_set_text - Update the label's text
 * @lbl:  Label to update
 * @text: New text (copied)
 */
void label_set_text(label_t *lbl, const char *text);

/**
 * label_destroy - Free label resources
 * @lbl: Label to destroy
 */
void label_destroy(label_t *lbl);

#endif /* LABEL_H */
