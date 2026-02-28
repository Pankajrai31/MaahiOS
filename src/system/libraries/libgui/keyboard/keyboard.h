/**
 * MaahiOS GUI Library - Keyboard Input Header
 * 
 * Description:
 *   Provides keyboard event reading for GUI applications.
 *   Reads key events from the keyboard device via SYS_DEV_READ.
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#ifndef GUI_KEYBOARD_H
#define GUI_KEYBOARD_H

#include <stdint.h>

/*=============================================================================
 * KEY EVENT TYPES
 *===========================================================================*/

#define KEY_PRESSED             1
#define KEY_RELEASED            2

/*=============================================================================
 * SCANCODES (PS/2 Set 1 - common keys)
 *===========================================================================*/

#define SC_ESCAPE               0x01
#define SC_1                    0x02
#define SC_2                    0x03
#define SC_3                    0x04
#define SC_4                    0x05
#define SC_5                    0x06
#define SC_6                    0x07
#define SC_7                    0x08
#define SC_8                    0x09
#define SC_9                    0x0A
#define SC_0                    0x0B
#define SC_MINUS                0x0C
#define SC_EQUALS               0x0D
#define SC_BACKSPACE            0x0E
#define SC_TAB                  0x0F
#define SC_ENTER                0x1C
#define SC_LCTRL                0x1D
#define SC_LSHIFT               0x2A
#define SC_RSHIFT               0x36
#define SC_LALT                 0x38
#define SC_SPACE                0x39
#define SC_CAPSLOCK             0x3A
#define SC_F1                   0x3B
#define SC_F2                   0x3C
#define SC_F3                   0x3D
#define SC_F4                   0x3E
#define SC_F5                   0x3F
#define SC_F6                   0x40
#define SC_F7                   0x41
#define SC_F8                   0x42
#define SC_F9                   0x43
#define SC_F10                  0x44
#define SC_UP                   0x48
#define SC_LEFT                 0x4B
#define SC_RIGHT                0x4D
#define SC_DOWN                 0x50
#define SC_DELETE               0x53

/*=============================================================================
 * MODIFIER FLAGS
 *===========================================================================*/

#define MOD_SHIFT               0x01
#define MOD_CTRL                0x02
#define MOD_ALT                 0x04
#define MOD_CAPSLOCK            0x08

/*=============================================================================
 * KEY EVENT STRUCTURE (must match kernel keyboard.h exactly)
 *===========================================================================*/

typedef struct {
    uint8_t type;               /* KEY_PRESSED or KEY_RELEASED */
    uint8_t keycode;            /* Internal keycode */
    uint8_t ascii;              /* ASCII value (0 if non-printable) */
    uint8_t modifiers;          /* Active modifiers (MOD_*) */
    uint8_t scancode;           /* Raw PS/2 scancode */
} __attribute__((packed)) key_event_t;

/*=============================================================================
 * KEYBOARD FUNCTIONS
 *===========================================================================*/

/**
 * kbd_read_event - Read a key event from the keyboard device
 * @evt: Output key event structure
 * 
 * Non-blocking. Returns >0 if an event was read, 0 if no event pending.
 * Returns negative on error.
 */
int kbd_read_event(key_event_t *evt);

/**
 * kbd_is_printable - Check if a key event has a printable ASCII character
 * @evt: Key event to check
 * 
 * Returns: 1 if printable (ASCII 32-126), 0 if not
 */
int kbd_is_printable(const key_event_t *evt);

#endif /* GUI_KEYBOARD_H */
