/**
 * MaahiOS Window Library - Button Control Implementation (Design System v2)
 *
 * Description:
 *   Renders 3D embossed buttons matching docs/maahi-os-design-system-v2.html.
 *
 *   Style  | Background          | Bevel          | Text  | Extra
 *   -------+---------------------+----------------+-------+-----------
 *   Std    | Chrome              | chrome bevel   | dark  |
 *   Default| Chrome              | chrome bevel   | dark  | teal glow, bold*
 *   Accent | Blue gradient ↓     | blue bevel     | white |
 *   Flat   | transparent (normal)| none (normal)  | dark  | hover shows
 *   Success| Green               | green bevel    | white |
 *   Danger | Red                 | red bevel      | white |
 *
 *   All: On press bevel inverts (sunken) + text shifts +1,+1.
 *   Size variants: Normal (5px/20px), Small (2px/10px), Large (7px/28px).
 *
 * Author: MaahiOS Team
 * Date: March 2026
 */

#include "button.h"
#include "../surface.h"
#include "../theme.h"

/* Forward declaration — user-space malloc/free */
extern void *malloc(unsigned int size);
extern void  free(void *ptr);

/*=============================================================================
 * STYLE LOOKUP TABLE  (V2: embossed 3D with bevel edges)
 *===========================================================================*/

typedef struct {
    uint32_t bg;            /* Face color                                */
    uint32_t fg;            /* Text color                                */
    uint32_t bevel_light;   /* Top/left edge when raised                 */
    uint32_t bevel_dark;    /* Bottom/right edge when raised             */
    uint32_t glow;          /* Outer outline (default btn only)          */
    int      has_glow;      /* 1 = draw 1px outer glow rectangle        */
    int      has_gradient;  /* 1 = vertical gradient from bg to bg_end   */
    uint32_t bg_end;        /* Gradient bottom color (if has_gradient)   */
    int      transparent;   /* 1 = no bg fill in normal state (Flat btn) */
} button_theme_t;

static const button_theme_t button_themes[] = {
    /* BTN_STANDARD → chrome bg, raised bevel, dark text */
    {
        .bg          = THEME_BTN_STD_BG,
        .fg          = THEME_BTN_STD_FG,
        .bevel_light = THEME_BEVEL_LIGHT,
        .bevel_dark  = THEME_BEVEL_DARK,
        .glow        = 0,
        .has_glow    = 0,
        .has_gradient = 0,
        .bg_end      = 0,
        .transparent = 0,
    },
    /* BTN_ACCENT → blue vertical gradient, blue bevel, white text */
    {
        .bg          = THEME_ACCENT_LIGHT,
        .fg          = THEME_BTN_ACCENT_FG,
        .bevel_light = THEME_ACCENT_LIGHT,
        .bevel_dark  = THEME_ACCENT_DARK,
        .glow        = 0,
        .has_glow    = 0,
        .has_gradient = 1,
        .bg_end      = THEME_ACCENT_DARK,
        .transparent = 0,
    },
    /* BTN_DEFAULT → chrome + teal glow outline + bold text */
    {
        .bg          = THEME_BTN_DEFAULT_BG,
        .fg          = THEME_BTN_DEFAULT_FG,
        .bevel_light = THEME_BEVEL_LIGHT,
        .bevel_dark  = THEME_BEVEL_DARK,
        .glow        = THEME_BTN_DEFAULT_GLOW,
        .has_glow    = 1,
        .has_gradient = 0,
        .bg_end      = 0,
        .transparent = 0,
    },
    /* BTN_FLAT → transparent, no bevel normal. Hover = chrome + bevel */
    {
        .bg          = THEME_CHROME_LIGHT,
        .fg          = THEME_BTN_FLAT_FG,
        .bevel_light = THEME_BEVEL_LIGHT,
        .bevel_dark  = THEME_BEVEL_DARK,
        .glow        = 0,
        .has_glow    = 0,
        .has_gradient = 0,
        .bg_end      = 0,
        .transparent = 1,
    },
    /* BTN_SUCCESS → green bg, green bevel, white text */
    {
        .bg          = THEME_BTN_SUCCESS_BG,
        .fg          = THEME_BTN_SUCCESS_FG,
        .bevel_light = 0x0053D769,     /* Lighter green highlight     */
        .bevel_dark  = 0x001E7E34,     /* Darker green shadow         */
        .glow        = 0,
        .has_glow    = 0,
        .has_gradient = 0,
        .bg_end      = 0,
        .transparent = 0,
    },
    /* BTN_DANGER → red bg, red bevel, white text */
    {
        .bg          = THEME_BTN_DANGER_BG,
        .fg          = THEME_BTN_DANGER_FG,
        .bevel_light = 0x00E8606E,     /* Lighter red highlight       */
        .bevel_dark  = 0x00A32835,     /* Darker red shadow           */
        .glow        = 0,
        .has_glow    = 0,
        .has_gradient = 0,
        .bg_end      = 0,
        .transparent = 0,
    },
};

/*=============================================================================
 * INTERNAL: COLOR LERP (for gradient buttons)
 *===========================================================================*/

static uint32_t btn_lerp_color(uint32_t c0, uint32_t c1, int pos, int total) {
    if (total <= 1) return c0;
    int r0 = (c0 >> 16) & 0xFF, g0 = (c0 >> 8) & 0xFF, b0 = c0 & 0xFF;
    int r1 = (c1 >> 16) & 0xFF, g1 = (c1 >> 8) & 0xFF, b1 = c1 & 0xFF;
    int r = r0 + (r1 - r0) * pos / (total - 1);
    int g = g0 + (g1 - g0) * pos / (total - 1);
    int b = b0 + (b1 - b0) * pos / (total - 1);
    return (uint32_t)(((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF));
}

/*=============================================================================
 * VTABLE FUNCTIONS
 *===========================================================================*/

static void button_draw(control_t *ctrl, surface_t *surf) {
    button_t *btn = (button_t *)ctrl;
    int style_idx = (int)btn->style;
    if (style_idx < 0 || style_idx >= (int)(sizeof(button_themes) / sizeof(button_themes[0]))) {
        style_idx = 0;
    }

    const button_theme_t *theme = &button_themes[style_idx];

    int x = ctrl->x;
    int y = ctrl->y;
    int w = ctrl->width;
    int h = ctrl->height;

    int is_pressed = (btn->state == BTN_STATE_PRESSED && ctrl->enabled);
    int is_hover   = (btn->state == BTN_STATE_HOVER   && ctrl->enabled);
    int is_disabled = !ctrl->enabled;

    /* ---- Disabled state: flat chrome, muted text, no depth ---- */
    if (is_disabled) {
        surface_fill_rect(surf, x, y, w, h, THEME_CHROME_DARK);
        /* Flat 2px border (no 3D effect) using a single muted color */
        uint32_t border_c = THEME_BEVEL_DARK;
        surface_draw_hline(surf, x, y, w, border_c);
        surface_draw_hline(surf, x + 1, y + 1, w - 2, border_c);
        surface_draw_vline(surf, x, y, h, border_c);
        surface_draw_vline(surf, x + 1, y + 1, h - 2, border_c);
        surface_draw_hline(surf, x, y + h - 1, w, border_c);
        surface_draw_hline(surf, x + 1, y + h - 2, w - 2, border_c);
        surface_draw_vline(surf, x + w - 1, y, h, border_c);
        surface_draw_vline(surf, x + w - 2, y + 1, h - 2, border_c);

        /* Centered text in disabled color */
        int text_w = surface_measure_text(btn->label, FONT_SMALL);
        int text_x = x + (w - text_w) / 2;
        int text_y = y + (h - surface_text_height(FONT_SMALL)) / 2;
        surface_draw_text(surf, text_x, text_y,
                          btn->label, FONT_SMALL, THEME_TEXT_DISABLED);
        return;
    }

    /* ---- Flat button: transparent in normal state ---- */
    if (theme->transparent && !is_hover && !is_pressed) {
        /* Don't fill background — leave parent bg visible.
         * Just draw the text, no bevel. */
        int text_w = surface_measure_text(btn->label, FONT_SMALL);
        int text_x = x + (w - text_w) / 2;
        int text_y = y + (h - surface_text_height(FONT_SMALL)) / 2;
        surface_draw_text(surf, text_x, text_y,
                          btn->label, FONT_SMALL, theme->fg);
        return;
    }

    /* ---- Fill button face ---- */
    if (theme->has_gradient && !is_pressed) {
        /* Vertical gradient: bg (top) → bg_end (bottom), row by row */
        int bw = 2;  /* bevel border width */
        for (int row = bw; row < h - bw; row++) {
            uint32_t c = btn_lerp_color(theme->bg, theme->bg_end,
                                        row - bw, h - bw * 2);
            surface_draw_hline(surf, x + bw, y + row, w - bw * 2, c);
        }
    } else {
        /* Solid fill (also used for gradient btn when pressed) */
        uint32_t bg = is_pressed && theme->has_gradient
                      ? theme->bg_end  /* pressed gradient → darker solid */
                      : theme->bg;
        surface_fill_rect(surf, x, y, w, h, bg);
    }

    /* ---- 3D bevel border (raised or sunken) ---- */
    uint32_t tl_color, br_color;
    if (is_pressed) {
        /* Sunken: dark on top/left, light on bottom/right */
        tl_color = theme->bevel_dark;
        br_color = theme->bevel_light;
    } else {
        /* Raised: light on top/left, dark on bottom/right */
        tl_color = theme->bevel_light;
        br_color = theme->bevel_dark;
    }

    /* Outer bevel: 2 lines each edge */
    surface_draw_hline(surf, x, y, w, tl_color);              /* top 1   */
    surface_draw_hline(surf, x + 1, y + 1, w - 2, tl_color);  /* top 2   */
    surface_draw_vline(surf, x, y, h, tl_color);              /* left 1  */
    surface_draw_vline(surf, x + 1, y + 1, h - 2, tl_color);  /* left 2  */
    surface_draw_hline(surf, x, y + h - 1, w, br_color);      /* bot 1   */
    surface_draw_hline(surf, x + 1, y + h - 2, w - 2, br_color); /* bot 2*/
    surface_draw_vline(surf, x + w - 1, y, h, br_color);      /* right 1 */
    surface_draw_vline(surf, x + w - 2, y + 1, h - 2, br_color); /* right 2*/

    /* ---- Optional outer glow (Default button: 1px teal rectangle) ---- */
    if (theme->has_glow) {
        surface_draw_rect(surf, x - 1, y - 1, w + 2, h + 2,
                          theme->glow, 1);
    }

    /* ---- Draw label text, centered ---- */
    int text_w = surface_measure_text(btn->label, FONT_SMALL);
    int text_x = x + (w - text_w) / 2;
    int text_y = y + (h - surface_text_height(FONT_SMALL)) / 2;

    /* Shift text +1,+1 when pressed (classic 3D tactile feedback) */
    if (is_pressed) {
        text_x += 1;
        text_y += 1;
    }

    surface_draw_text(surf, text_x, text_y, btn->label, FONT_SMALL, theme->fg);

    /* ---- Focus rectangle (dashed look: dotted line around text) ---- */
    if (ctrl->focused) {
        /* Draw a 1px dotted rect inside the bevel, 3px inset from edges */
        int fx = x + 4;
        int fy = y + 4;
        int fw = w - 8;
        int fh = h - 8;
        /* Dotted horizontal lines (every other pixel) */
        for (int px = fx; px < fx + fw; px += 2) {
            uint32_t *p1 = (fy >= 0 && px >= 0) ? &surf->pixels[fy * surf->width + px] : (uint32_t *)0;
            uint32_t *p2 = (fy + fh - 1 >= 0 && px >= 0) ? &surf->pixels[(fy + fh - 1) * surf->width + px] : (uint32_t *)0;
            if (p1 && px < surf->width && fy < surf->height) *p1 = THEME_TEXT;
            if (p2 && px < surf->width && fy + fh - 1 < surf->height) *p2 = THEME_TEXT;
        }
        /* Dotted vertical lines (every other pixel) */
        for (int py = fy; py < fy + fh; py += 2) {
            uint32_t *p1 = (py >= 0 && fx >= 0) ? &surf->pixels[py * surf->width + fx] : (uint32_t *)0;
            uint32_t *p2 = (py >= 0 && fx + fw - 1 >= 0) ? &surf->pixels[py * surf->width + (fx + fw - 1)] : (uint32_t *)0;
            if (p1 && fx < surf->width && py < surf->height) *p1 = THEME_TEXT;
            if (p2 && fx + fw - 1 < surf->width && py < surf->height) *p2 = THEME_TEXT;
        }
    }
}

static int button_event(control_t *ctrl, gui_event_t *evt) {
    button_t *btn = (button_t *)ctrl;
    if (!ctrl->enabled) return 0;

    switch (evt->type) {
    case GUI_EVENT_MOUSE_ENTER:
        btn->state = BTN_STATE_HOVER;
        ctrl->dirty = 1;
        return 1;

    case GUI_EVENT_MOUSE_LEAVE:
        btn->state = BTN_STATE_NORMAL;
        ctrl->dirty = 1;
        return 1;

    case GUI_EVENT_MOUSE_DOWN:
        if (evt->mouse_button == 0) {   /* Left click */
            btn->state = BTN_STATE_PRESSED;
            ctrl->dirty = 1;
            return 1;
        }
        break;

    case GUI_EVENT_MOUSE_UP:
        if (evt->mouse_button == 0 && btn->state == BTN_STATE_PRESSED) {
            btn->state = BTN_STATE_HOVER;
            ctrl->dirty = 1;
            /* Fire callback */
            if (btn->on_click) {
                btn->on_click(btn->click_data);
            }
            return 1;
        }
        break;

    default:
        break;
    }

    return 0;
}

static void button_destroy_impl(control_t *ctrl) {
    (void)ctrl;
    /* No dynamic resources inside button beyond the struct itself */
}

static const control_ops_t button_ops = {
    .draw    = button_draw,
    .event   = button_event,
    .destroy = button_destroy_impl,
};

/*=============================================================================
 * INTERNAL: SIZE PARAMETERS
 *===========================================================================*/

static void get_size_params(button_size_t size, int *pad_x, int *pad_y,
                            int *min_w, int *min_h) {
    switch (size) {
    case BTN_SIZE_SMALL:
        *pad_x = THEME_BTN_PAD_X_SM;   /* 10 */
        *pad_y = 2;
        *min_w = 50;
        *min_h = THEME_BTN_HEIGHT_SM;   /* 20 */
        break;
    case BTN_SIZE_LARGE:
        *pad_x = THEME_BTN_PAD_X_LG;   /* 28 */
        *pad_y = 7;
        *min_w = 100;
        *min_h = THEME_BTN_HEIGHT_LG;   /* 30 */
        break;
    default: /* BTN_SIZE_NORMAL */
        *pad_x = THEME_BTN_PAD_X;      /* 20 */
        *pad_y = 5;
        *min_w = THEME_BTN_MIN_WIDTH;   /* 75 */
        *min_h = THEME_BTN_HEIGHT;      /* 25 */
        break;
    }
}

/*=============================================================================
 * PUBLIC API
 *===========================================================================*/

button_t *button_create(int x, int y, int w, int h,
                        const char *label, button_style_t style) {
    button_t *btn = (button_t *)malloc(sizeof(button_t));
    if (!btn) return (button_t *)0;

    /* Copy label */
    int i;
    for (i = 0; i < BUTTON_MAX_LABEL - 1 && label && label[i]; i++) {
        btn->label[i] = label[i];
    }
    btn->label[i] = '\0';

    /* Default size = Normal */
    btn->size = BTN_SIZE_NORMAL;

    /* Auto-size if w or h is 0 */
    if (w <= 0) {
        int pad_x, pad_y, min_w, min_h;
        get_size_params(btn->size, &pad_x, &pad_y, &min_w, &min_h);
        w = surface_measure_text(btn->label, FONT_SMALL) + pad_x * 2;
        if (w < min_w) w = min_w;
    }
    if (h <= 0) {
        h = THEME_BTN_HEIGHT;
    }

    /* Initialize base control */
    CONTROL_INIT(&btn->base, CONTROL_BUTTON, x, y, w, h, &button_ops);

    /* Button-specific fields */
    btn->style      = style;
    btn->state      = BTN_STATE_NORMAL;
    btn->on_click   = (void (*)(void *))0;
    btn->click_data = (void *)0;

    return btn;
}

void button_set_label(button_t *btn, const char *label) {
    if (!btn) return;

    int i;
    for (i = 0; i < BUTTON_MAX_LABEL - 1 && label && label[i]; i++) {
        btn->label[i] = label[i];
    }
    btn->label[i] = '\0';
    btn->base.dirty = 1;
}

void button_set_size(button_t *btn, button_size_t size) {
    if (!btn) return;
    btn->size = size;

    /* Recalculate dimensions from label + new padding */
    int pad_x, pad_y, min_w, min_h;
    get_size_params(size, &pad_x, &pad_y, &min_w, &min_h);

    int w = surface_measure_text(btn->label, FONT_SMALL) + pad_x * 2;
    if (w < min_w) w = min_w;
    int h = min_h;

    btn->base.width  = w;
    btn->base.height = h;
    btn->base.dirty  = 1;
}

void button_set_on_click(button_t *btn,
                         void (*callback)(void *userdata),
                         void *userdata) {
    if (!btn) return;
    btn->on_click   = callback;
    btn->click_data = userdata;
}

void button_destroy(button_t *btn) {
    if (!btn) return;
    if (btn->base.ops->destroy) {
        btn->base.ops->destroy(&btn->base);
    }
    free(btn);
}
