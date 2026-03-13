/**
 * MaahiOS Window Library - Theme Constants (Design System v2)
 *
 * Description:
 *   All visual constants for the MaahiOS Design System v2.
 *   Embossed Light Theme with 3D beveled borders.
 *   Colors, sizes, and spacing from docs/maahi-os-design-system-v2.html.
 *
 *   This is a pure header with #defines — no runtime overhead,
 *   no theme executive needed. Compiled into libwindow.
 *
 *   Color format: 0x00RRGGBB (matches framebuffer pixel format)
 *
 * Author: MaahiOS Team
 * Date: March 2026
 */

#ifndef THEME_H
#define THEME_H

/*=============================================================================
 * CHROME (System UI Base)
 *
 * The light blue-gray chrome is the primary background for all system UI
 * elements: window borders, buttons, taskbar, toolbars, dialogs.
 *===========================================================================*/

#define THEME_CHROME            0x00D8DBE8
#define THEME_CHROME_LIGHT      0x00E8EAF2
#define THEME_CHROME_LIGHTER    0x00F0F1F6
#define THEME_CHROME_DARK       0x00C0C4D4
#define THEME_CHROME_DARKER     0x00B0B4C6

/*=============================================================================
 * 3D BEVEL COLORS
 *
 * Core visual language. Raised = interactive/clickable (buttons, toolbars).
 * Sunken = content well / input. Creates the embossed depth effect.
 *   Raised: top/left = BEVEL_LIGHT, bottom/right = BEVEL_DARK
 *   Sunken: top/left = BEVEL_DARK,  bottom/right = BEVEL_LIGHT
 *===========================================================================*/

#define THEME_BEVEL_LIGHT       0x00F4F5FA
#define THEME_BEVEL_DARK        0x009498AC

/*=============================================================================
 * ACCENT COLORS
 *===========================================================================*/

#define THEME_ACCENT            0x002B5BB5  /* Primary accent (selection)   */
#define THEME_ACCENT_LIGHT      0x004A7BD5
#define THEME_ACCENT_DARK       0x001B4A9A
#define THEME_TEAL              0x001E8A65  /* Secondary accent             */
#define THEME_TEAL_DARK         0x00156B4E
#define THEME_CYAN              0x0018A080  /* Highlight / glow             */

/*=============================================================================
 * STATUS COLORS
 *===========================================================================*/

#define THEME_SUCCESS           0x0028A745
#define THEME_WARNING           0x00E8A317
#define THEME_DANGER            0x00DC3545
#define THEME_INFO              0x0017A2B8

/*=============================================================================
 * TEXT & SURFACES
 *===========================================================================*/

#define THEME_WHITE             0x00FFFFFF
#define THEME_BLACK             0x00000000

#define THEME_TEXT              0x001A1A2E  /* Primary body text            */
#define THEME_TEXT_SECONDARY    0x005A5D76  /* Muted/secondary text         */
#define THEME_TEXT_DISABLED     0x009CA0B4  /* Disabled/placeholder         */
#define THEME_TEXT_INVERSE      0x00FFFFFF  /* Text on dark/blue bg         */

#define THEME_SURFACE           0x00FFFFFF  /* Window body / content area   */
#define THEME_SURFACE_RAISED    0x00D8DBE8  /* Raised surface = chrome      */
#define THEME_SURFACE_SUNKEN    0x00C8CBD8  /* Sunken/inset surface         */
#define THEME_INPUT_BG          0x00FFFFFF  /* Input field background       */

/* Legacy aliases (backward compat with existing code) */
#define THEME_TEXT_DARK         THEME_TEXT
#define THEME_TEXT_MUTED        THEME_TEXT_DISABLED
#define THEME_PRIMARY_BLUE      THEME_ACCENT
#define THEME_PRIMARY_DARK      0x00131A22
#define THEME_DISABLED_BG       THEME_CHROME_DARK
#define THEME_DISABLED_FG       THEME_TEXT_DISABLED
#define THEME_BORDER            THEME_BEVEL_DARK

/*=============================================================================
 * WINDOW CHROME
 *===========================================================================*/

/* Titlebar — horizontal gradient from deep blue to accent blue */
#define THEME_TITLEBAR_START    0x001B3F8B  /* Gradient left                */
#define THEME_TITLEBAR_END      0x002B5BB5  /* Gradient right               */
#define THEME_TITLEBAR_INACT_S  0x009498AC  /* Inactive start               */
#define THEME_TITLEBAR_INACT_E  0x00B0B4C6  /* Inactive end                 */
#define THEME_TITLEBAR_FG       THEME_TEXT_INVERSE
#define THEME_TITLEBAR_HEIGHT   24          /* pixels                       */

/* Legacy alias for old code referencing THEME_TITLEBAR_BG */
#define THEME_TITLEBAR_BG       THEME_TITLEBAR_START

/* Window body */
#define THEME_WINDOW_BG         THEME_SURFACE
#define THEME_WINDOW_BORDER     THEME_BEVEL_DARK
#define THEME_WINDOW_PADDING    8

/* Titlebar buttons (─ □ ✕ on right side, chrome raised) */
#define THEME_TITLEBAR_BTN_W    20
#define THEME_TITLEBAR_BTN_H    18
#define THEME_TITLEBAR_BTN_GAP  2

/*=============================================================================
 * BUTTON STYLES (Embossed 3D)
 *
 * All buttons use raised 3D bevel borders. On press, the bevel inverts
 * to sunken and text shifts +1,+1 for tactile feedback.
 *===========================================================================*/

/* Standard button: chrome bg, raised bevel, dark text */
#define THEME_BTN_STD_BG        THEME_CHROME
#define THEME_BTN_STD_FG        THEME_TEXT

/* Default button: standard + teal outline glow + bold */
#define THEME_BTN_DEFAULT_BG    THEME_CHROME
#define THEME_BTN_DEFAULT_FG    THEME_TEXT
#define THEME_BTN_DEFAULT_GLOW  THEME_TEAL

/* Accent button: blue bg, blue bevel, white text */
#define THEME_BTN_ACCENT_BG     THEME_ACCENT
#define THEME_BTN_ACCENT_FG     THEME_TEXT_INVERSE

/* Flat button (toolbar): transparent, border on hover */
#define THEME_BTN_FLAT_FG       THEME_TEXT

/* Success button: green variant */
#define THEME_BTN_SUCCESS_BG    THEME_SUCCESS
#define THEME_BTN_SUCCESS_FG    THEME_TEXT_INVERSE

/* Danger button: red variant */
#define THEME_BTN_DANGER_BG     THEME_DANGER
#define THEME_BTN_DANGER_FG     THEME_TEXT_INVERSE

/* Button sizing (matching V2 design: 5px 20px padding, 25px default) */
#define THEME_BTN_HEIGHT        25
#define THEME_BTN_HEIGHT_SM     20
#define THEME_BTN_HEIGHT_LG     30
#define THEME_BTN_MIN_WIDTH     75
#define THEME_BTN_PAD_X         20
#define THEME_BTN_PAD_X_SM      10
#define THEME_BTN_PAD_X_LG      28
#define THEME_BTN_BORDER_W      2

/*=============================================================================
 * FORM CONTROLS (future)
 *===========================================================================*/

#define THEME_INPUT_BORDER      THEME_BEVEL_DARK
#define THEME_INPUT_FOCUS       THEME_ACCENT
#define THEME_INPUT_HEIGHT      24
#define THEME_INPUT_PAD_X       6
#define THEME_CHECK_SIZE        13

/*=============================================================================
 * PANELS
 *===========================================================================*/

#define THEME_PANEL_BG          THEME_CHROME
#define THEME_PANEL_BORDER      THEME_BEVEL_DARK

/*=============================================================================
 * TASKBAR (used by Orbit)
 *===========================================================================*/

#define THEME_TASKBAR_BG        THEME_CHROME
#define THEME_TASKBAR_HEIGHT    32

/*=============================================================================
 * DESKTOP BACKGROUND (blue gradient)
 *===========================================================================*/

#define THEME_DESKTOP_TOP       0x003A7BB8
#define THEME_DESKTOP_MID       0x005A9BD8
#define THEME_DESKTOP_BOT       0x003A8BC8

/*=============================================================================
 * FONT — Legacy 8x16 bitmap
 *===========================================================================*/

#define THEME_FONT_WIDTH        8
#define THEME_FONT_HEIGHT       16

/*=============================================================================
 * FONT — Proportional anti-aliased (Segoe UI via libfont)
 *
 * Use these with surface_draw_text() / surface_measure_text() / libfont API.
 * Values match font_size_t enum in libfont.h.
 *===========================================================================*/

#include "../libgui/fonts/libfont.h"

#define THEME_FONT_TITLE        FONT_TITLE   /* 24px — window titles, hero  */
#define THEME_FONT_H2           FONT_H2      /* 18px — section headings     */
#define THEME_FONT_H3           FONT_H3      /* 16px — sub-headings         */
#define THEME_FONT_BODY         FONT_BODY    /* 14px — body text, buttons   */
#define THEME_FONT_SMALL        FONT_SMALL   /* 12px — captions, labels     */

#endif /* THEME_H */
