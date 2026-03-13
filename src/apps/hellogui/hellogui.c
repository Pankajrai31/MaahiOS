/**
 * hellogui.mex - MaahiOS Hello GUI Demo (Design System v2)
 *
 * Test app for the libwindow GUI system.
 * Demonstrates button styles, disabled state, window drag, and clean close.
 *
 * Uses: libwindow (window + controls)
 */

#include "../../system/libraries/libwindow/libwindow.h"
#include "../../system/libraries/libgui/libgui.h"

/*=============================================================================
 * GLOBALS & CALLBACKS
 *===========================================================================*/

static label_t *g_status_label = (label_t *)0;
static window_t *g_win = (window_t *)0;

static void on_hello_click(void *userdata) {
    (void)userdata;
    if (g_status_label) {
        label_set_text(g_status_label, "Hello from MaahiOS!");
    }
}

static void on_reset_click(void *userdata) {
    (void)userdata;
    if (g_status_label) {
        label_set_text(g_status_label, "Click a button...");
    }
}

static void on_close_click(void *userdata) {
    (void)userdata;
    if (g_win) {
        window_close(g_win);
    }
}

/*=============================================================================
 * ENTRY POINT
 *===========================================================================*/

void mex_main(void) {
    /* Center window dynamically on whatever screen resolution */
    int win_w = 360, win_h = 240;
    int scr_w = (int)gui_get_screen_width();
    int scr_h = (int)gui_get_screen_height();
    int win_x = (scr_w - win_w) / 2;
    int win_y = (scr_h - win_h) / 2;

    window_t *win = window_create("Hello GUI", win_x, win_y, win_w, win_h);
    if (!win) return;
    g_win = win;

    /* Status label at top */
    g_status_label = label_create(16, 8, "Click a button...", THEME_TEXT);
    if (g_status_label) {
        window_add_control(win, &g_status_label->base);
    }

    /* ---- Row 1: Standard | Default | Flat  (y=32) ---- */
    int y1 = 32;

    button_t *btn_std = button_create(16, y1, 0, 0, "Standard", BTN_STANDARD);
    if (btn_std) {
        button_set_on_click(btn_std, on_hello_click, (void *)0);
        window_add_control(win, &btn_std->base);
    }

    button_t *btn_def = button_create(136, y1, 0, 0, "Default", BTN_DEFAULT);
    if (btn_def) {
        button_set_on_click(btn_def, on_reset_click, (void *)0);
        window_add_control(win, &btn_def->base);
    }

    button_t *btn_flat = button_create(264, y1, 0, 0, "Flat", BTN_FLAT);
    if (btn_flat) {
        button_set_on_click(btn_flat, on_reset_click, (void *)0);
        window_add_control(win, &btn_flat->base);
    }

    /* ---- Row 2: Success | Danger | Disabled  (y=68) ---- */
    int y2 = 68;

    button_t *btn_ok = button_create(16, y2, 0, 0, "Success", BTN_SUCCESS);
    if (btn_ok) {
        button_set_on_click(btn_ok, on_hello_click, (void *)0);
        window_add_control(win, &btn_ok->base);
    }

    button_t *btn_danger = button_create(136, y2, 0, 0, "Danger", BTN_DANGER);
    if (btn_danger) {
        button_set_on_click(btn_danger, on_reset_click, (void *)0);
        window_add_control(win, &btn_danger->base);
    }

    button_t *btn_disabled = button_create(264, y2, 0, 0, "Disabled", BTN_STANDARD);
    if (btn_disabled) {
        btn_disabled->base.enabled = 0;
        window_add_control(win, &btn_disabled->base);
    }

    /* ---- Row 3: Close button  (y=120) ---- */
    label_t *lbl_info = label_create(16, 118, "Drag titlebar to move.", THEME_TEXT_SECONDARY);
    if (lbl_info) window_add_control(win, &lbl_info->base);

    button_t *btn_close = button_create(16, 140, 320, 0, "Close Window", BTN_DEFAULT);
    if (btn_close) {
        button_set_on_click(btn_close, on_close_click, (void *)0);
        window_add_control(win, &btn_close->base);
    }

    /* Run the event loop (blocks until close button, ESC, or X) */
    window_run(win);

    /* Cleanup — then mex_entry.s calls SYS_EXIT for clean process exit */
    window_destroy(win);
}
