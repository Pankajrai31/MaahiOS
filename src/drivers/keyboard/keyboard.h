/**
 * MaahiOS PS/2 Keyboard Driver
 * 
 * Provides keyboard input handling via IRQ1.
 * Registers with Device Manager for unified device access.
 */

#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include <stddef.h>

/* ============================================
 * Key Event Types
 * ============================================ */
#define KEY_PRESSED  1
#define KEY_RELEASED 2

/* ============================================
 * Modifier Flags
 * ============================================ */
#define MOD_SHIFT   0x01
#define MOD_CTRL    0x02
#define MOD_ALT     0x04
#define MOD_CAPS    0x08

/* ============================================
 * Special Scancodes
 * ============================================ */
#define SC_ESCAPE    0x01
#define SC_BACKSPACE 0x0E
#define SC_TAB       0x0F
#define SC_ENTER     0x1C
#define SC_CTRL      0x1D
#define SC_LSHIFT    0x2A
#define SC_RSHIFT    0x36
#define SC_ALT       0x38
#define SC_SPACE     0x39
#define SC_CAPSLOCK  0x3A
#define SC_F1        0x3B
#define SC_F2        0x3C
#define SC_F3        0x3D
#define SC_F4        0x3E
#define SC_F5        0x3F
#define SC_F6        0x40
#define SC_F7        0x41
#define SC_F8        0x42
#define SC_F9        0x43
#define SC_F10       0x44
#define SC_UP        0x48
#define SC_LEFT      0x4B
#define SC_RIGHT     0x4D
#define SC_DOWN      0x50

/* ============================================
 * Key Event Structure
 * ============================================ */
typedef struct {
    uint8_t type;       /* KEY_PRESSED or KEY_RELEASED */
    uint8_t keycode;    /* Scancode without release bit */
    uint8_t ascii;      /* ASCII character (0 if none) */
    uint8_t modifiers;  /* Active modifiers at time of event */
    uint8_t scancode;   /* Raw scancode from keyboard */
} key_event_t;

/* ============================================
 * Driver API
 * ============================================ */

/**
 * Initialize keyboard driver.
 * Registers with Device Manager as DEV_KEYBOARD.
 */
int keyboard_init(void);

/**
 * Keyboard IRQ1 handler.
 * Called by interrupt stub.
 */
void keyboard_irq_handler(void);

#endif /* KEYBOARD_H */
