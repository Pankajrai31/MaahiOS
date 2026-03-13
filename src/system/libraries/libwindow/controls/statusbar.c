/**
 * MaahiOS Window Library - StatusBar Control Implementation (Design System v2)
 *
 * Description:
 *   Renders a dark status strip at the window bottom with text panels
 *   separated by subtle vertical lines.
 *
 *   Background: THEME_PRIMARY_DARK (#131A22) — dark navy
 *   Text: white (FONT_SMALL)
 *   Separator: subtle blended line
 *
 * Author: MaahiOS Team
 * Date: March 2026
 */

#include "statusbar.h"
#include "../surface.h"
#include "../theme.h"

/* Forward declaration — user-space malloc/free */
extern void *malloc(unsigned int size);
extern void  free(void *ptr);

/*=============================================================================
 * THEME COLORS
 *===========================================================================*/

#define SB_BG           THEME_PRIMARY_DARK      /* #131A22 dark navy        */
#define SB_FG           THEME_TEXT_INVERSE       /* White text               */
#define SB_SEP          0x002A3444               /* Subtle separator line    */

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

static void sb_strcpy(char *dst, const char *src, int max) {
    int i;
    for (i = 0; i < max - 1 && src && src[i]; i++)
        dst[i] = src[i];
    dst[i] = '\0';
}

/*=============================================================================
 * VTABLE FUNCTIONS
 *===========================================================================*/

static void statusbar_draw(control_t *ctrl, surface_t *surf) {
    statusbar_t *sb = (statusbar_t *)ctrl;
    int x = ctrl->x;
    int y = ctrl->y;
    int w = ctrl->width;
    int h = STATUSBAR_HEIGHT;

    /* Dark background */
    surface_fill_rect(surf, x, y, w, h, SB_BG);

    /* Top border line (subtle) */
    surface_draw_hline(surf, x, y, w, SB_SEP);

    /* Draw panels */
    int cx = x + STATUSBAR_PAD_X;
    int th = surface_text_height(FONT_SMALL);
    int text_y = y + (h - th) / 2;
    int i;

    for (i = 0; i < sb->panel_count; i++) {
        statusbar_panel_t *panel = &sb->panels[i];

        /* Draw panel text */
        if (panel->text[0] != '\0') {
            surface_draw_text(surf, cx, text_y, panel->text,
                              FONT_SMALL, SB_FG);
        }

        /* Advance past panel width */
        int pw;
        if (panel->width > 0) {
            pw = panel->width;
        } else {
            /* Auto: measure text + padding */
            pw = surface_measure_text(panel->text, FONT_SMALL) + STATUSBAR_PAD_X * 2;
        }
        cx += pw;

        /* Separator line (except after last panel) */
        if (i < sb->panel_count - 1) {
            surface_draw_vline(surf, cx, y + 4, h - 8, SB_SEP);
            cx += STATUSBAR_PAD_X;
        }
    }
}

static int statusbar_event(control_t *ctrl, gui_event_t *evt) {
    (void)ctrl;
    (void)evt;
    return 0;  /* Statusbar doesn't consume events */
}

static void statusbar_destroy_impl(control_t *ctrl) {
    (void)ctrl;
}

static const control_ops_t statusbar_ops = {
    .draw    = statusbar_draw,
    .event   = statusbar_event,
    .destroy = statusbar_destroy_impl,
};

/*=============================================================================
 * PUBLIC API
 *===========================================================================*/

statusbar_t *statusbar_create(int x, int y, int w) {
    statusbar_t *sb = (statusbar_t *)malloc(sizeof(statusbar_t));
    if (!sb) return (statusbar_t *)0;

    /* Zero-initialize */
    {
        unsigned char *p = (unsigned char *)sb;
        unsigned int i;
        for (i = 0; i < sizeof(statusbar_t); i++) p[i] = 0;
    }

    CONTROL_INIT(&sb->base, CONTROL_PANEL, x, y, w, STATUSBAR_HEIGHT,
                 &statusbar_ops);
    sb->panel_count = 0;

    return sb;
}

int statusbar_add_panel(statusbar_t *sb, const char *text, int width) {
    if (!sb || sb->panel_count >= STATUSBAR_MAX_PANELS) return -1;

    int idx = sb->panel_count;
    sb_strcpy(sb->panels[idx].text, text, STATUSBAR_MAX_TEXT);
    sb->panels[idx].width = width;
    sb->panel_count++;
    sb->base.dirty = 1;
    return idx;
}

void statusbar_set_text(statusbar_t *sb, int index, const char *text) {
    if (!sb || index < 0 || index >= sb->panel_count) return;
    sb_strcpy(sb->panels[index].text, text, STATUSBAR_MAX_TEXT);
    sb->base.dirty = 1;
}

void statusbar_destroy(statusbar_t *sb) {
    if (!sb) return;
    if (sb->base.ops->destroy) {
        sb->base.ops->destroy(&sb->base);
    }
    free(sb);
}
