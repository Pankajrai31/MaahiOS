/**
 * MaahiOS Window Library - Label Control Implementation
 * 
 * Description:
 *   Renders static text. The simplest control — draw-only, no events.
 * 
 * Author: MaahiOS Team
 * Date: March 2026
 */

#include "label.h"
#include "../surface.h"
#include "../theme.h"

/* Forward declaration — user-space malloc/free */
extern void *malloc(unsigned int size);
extern void  free(void *ptr);

/*=============================================================================
 * VTABLE FUNCTIONS
 *===========================================================================*/

static void label_draw(control_t *ctrl, surface_t *surf) {
    label_t *lbl = (label_t *)ctrl;

    if (!lbl->transparent) {
        /* Fill background behind text */
        surface_fill_rect(surf, ctrl->x, ctrl->y,
                          ctrl->width, ctrl->height, lbl->bg_color);
    }

    /* Draw text, vertically centered in the control height */
    int text_y = ctrl->y + (ctrl->height - THEME_FONT_HEIGHT) / 2;
    if (text_y < ctrl->y) text_y = ctrl->y;

    if (lbl->transparent) {
        surface_draw_string_transparent(surf, ctrl->x, text_y,
                                        lbl->text, lbl->fg_color);
    } else {
        surface_draw_string(surf, ctrl->x, text_y,
                            lbl->text, lbl->fg_color, lbl->bg_color);
    }
}

static int label_event(control_t *ctrl, gui_event_t *evt) {
    (void)ctrl;
    (void)evt;
    return 0;   /* Labels don't consume events */
}

static void label_destroy_impl(control_t *ctrl) {
    (void)ctrl;
    /* No dynamic resources inside label beyond the struct itself */
}

static const control_ops_t label_ops = {
    .draw    = label_draw,
    .event   = label_event,
    .destroy = label_destroy_impl,
};

/*=============================================================================
 * PUBLIC API
 *===========================================================================*/

label_t *label_create(int x, int y, const char *text, uint32_t fg) {
    label_t *lbl = (label_t *)malloc(sizeof(label_t));
    if (!lbl) return (label_t *)0;

    /* Copy text */
    int i;
    for (i = 0; i < LABEL_MAX_TEXT - 1 && text && text[i]; i++) {
        lbl->text[i] = text[i];
    }
    lbl->text[i] = '\0';

    /* Calculate dimensions */
    int text_w = surface_measure_string(lbl->text);
    int text_h = THEME_FONT_HEIGHT;

    /* Initialize base control */
    CONTROL_INIT(&lbl->base, CONTROL_LABEL, x, y, text_w, text_h, &label_ops);

    /* Label-specific defaults */
    lbl->fg_color    = fg;
    lbl->bg_color    = THEME_WINDOW_BG;
    lbl->transparent = 1;    /* Default: transparent background */

    return lbl;
}

void label_set_text(label_t *lbl, const char *text) {
    if (!lbl) return;

    int i;
    for (i = 0; i < LABEL_MAX_TEXT - 1 && text && text[i]; i++) {
        lbl->text[i] = text[i];
    }
    lbl->text[i] = '\0';

    /* Recalculate width */
    lbl->base.width = surface_measure_string(lbl->text);
    lbl->base.dirty = 1;
}

void label_destroy(label_t *lbl) {
    if (!lbl) return;
    if (lbl->base.ops->destroy) {
        lbl->base.ops->destroy(&lbl->base);
    }
    free(lbl);
}
