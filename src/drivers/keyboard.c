/*
 * MaahiOS Keyboard Driver
 * PS/2 keyboard support with scancode translation and event queue
 */

#include "keyboard.h"

/* Keyboard ports */
#define KBD_DATA_PORT    0x60
#define KBD_STATUS_PORT  0x64
#define KBD_CMD_PORT     0x64

/* Key event types */
#define KEY_PRESSED  1
#define KEY_RELEASED 2

/* Modifier flags */
#define MOD_SHIFT   0x01
#define MOD_CTRL    0x02
#define MOD_ALT     0x04
#define MOD_CAPS    0x08

/* Event queue */
#define EVENT_QUEUE_SIZE 32
static key_event_t event_queue[EVENT_QUEUE_SIZE];
static volatile int queue_head = 0;
static volatile int queue_tail = 0;
static volatile int queue_count = 0;

/* Modifier state */
static volatile unsigned char modifiers = 0;
static volatile int keyboard_initialized = 0;

/* US keyboard scancode to ASCII (lowercase) */
static const unsigned char scancode_to_ascii[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0
};

/* Shifted versions */
static const unsigned char scancode_to_ascii_shift[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0
};

/* Special scancodes */
#define SC_LSHIFT    0x2A
#define SC_RSHIFT    0x36
#define SC_CTRL      0x1D
#define SC_ALT       0x38
#define SC_CAPSLOCK  0x3A
#define SC_ESCAPE    0x01
#define SC_BACKSPACE 0x0E
#define SC_ENTER     0x1C
#define SC_TAB       0x0F
#define SC_SPACE     0x39

/* Serial debug */
static void kbd_serial_print(const char *str) {
    while (*str) {
        while ((*(volatile unsigned char*)0x3FD & 0x20) == 0);
        *(volatile unsigned char*)0x3F8 = *str++;
    }
}

/**
 * Initialize keyboard driver
 */
void keyboard_init(void) {
    /* Clear event queue */
    queue_head = 0;
    queue_tail = 0;
    queue_count = 0;
    modifiers = 0;
    
    /* Enable keyboard IRQ (IRQ1) */
    unsigned char mask;
    __asm__ volatile("inb $0x21, %0" : "=a"(mask));
    mask &= ~0x02;  /* Clear bit 1 to enable IRQ1 */
    __asm__ volatile("outb %0, $0x21" : : "a"(mask));
    
    keyboard_initialized = 1;
    kbd_serial_print("[KEYBOARD] Initialized\n");
}

/**
 * Add event to queue
 */
static void queue_add_event(unsigned char type, unsigned char keycode, 
                            unsigned char ascii, unsigned char scancode) {
    if (queue_count >= EVENT_QUEUE_SIZE) {
        return;  /* Queue full, drop event */
    }
    
    event_queue[queue_tail].type = type;
    event_queue[queue_tail].keycode = keycode;
    event_queue[queue_tail].modifiers = modifiers;
    event_queue[queue_tail].scancode = scancode;
    
    queue_tail = (queue_tail + 1) % EVENT_QUEUE_SIZE;
    queue_count++;
}

/**
 * IRQ1 handler - called from interrupt stub
 */
void keyboard_irq_handler(void) {
    /* Read scan code */
    unsigned char scancode;
    __asm__ volatile("inb $0x60, %0" : "=a"(scancode));
    
    /* Debug output */
    kbd_serial_print("[KBD] SC=");
    /* Print scancode as hex */
    static const char hex[] = "0123456789ABCDEF";
    char buf[3];
    buf[0] = hex[(scancode >> 4) & 0xF];
    buf[1] = hex[scancode & 0xF];
    buf[2] = '\0';
    kbd_serial_print(buf);
    kbd_serial_print("\n");
    
    /* Check if key release (bit 7 set) */
    int released = scancode & 0x80;
    unsigned char code = scancode & 0x7F;
    
    /* Handle modifier keys */
    if (code == SC_LSHIFT || code == SC_RSHIFT) {
        if (released) modifiers &= ~MOD_SHIFT;
        else modifiers |= MOD_SHIFT;
        return;
    }
    if (code == SC_CTRL) {
        if (released) modifiers &= ~MOD_CTRL;
        else modifiers |= MOD_CTRL;
        return;
    }
    if (code == SC_ALT) {
        if (released) modifiers &= ~MOD_ALT;
        else modifiers |= MOD_ALT;
        return;
    }
    if (code == SC_CAPSLOCK && !released) {
        modifiers ^= MOD_CAPS;  /* Toggle caps lock */
        return;
    }
    
    /* *** DEBUG: Ctrl+Alt+Shift = Dump klog (safe from Windows host conflicts) *** */
    if ((modifiers & MOD_CTRL) && (modifiers & MOD_ALT) && (modifiers & MOD_SHIFT)) {
        /* On ANY key press with all 3 modifiers, dump klog */
        if (!released && code != SC_CTRL && code != SC_ALT && code != SC_LSHIFT && code != SC_RSHIFT) {
            kbd_serial_print("\n\n[MANUAL DUMP] Ctrl+Alt+Shift triggered\n");
            extern void klog_dump(void);
            klog_dump();
            kbd_serial_print("[MANUAL DUMP] Complete\n\n");
            return;
        }
    }
    
    /* Get ASCII character */
    unsigned char ascii;
    if (modifiers & (MOD_SHIFT | MOD_CAPS)) {
        ascii = scancode_to_ascii_shift[code];
    } else {
        ascii = scancode_to_ascii[code];
    }
    
    /* Add to event queue */
    if (!released) {
        queue_add_event(KEY_PRESSED, code, ascii, scancode);
    } else {
        queue_add_event(KEY_RELEASED, code, ascii, scancode);
    }
}

/**
 * Check if key event is available
 */
int keyboard_has_event(void) {
    return queue_count > 0;
}

/**
 * Get next key event from queue
 */
int keyboard_get_event(key_event_t *event) {
    if (queue_count == 0) {
        return 0;
    }
    
    *event = event_queue[queue_head];
    queue_head = (queue_head + 1) % EVENT_QUEUE_SIZE;
    queue_count--;
    
    return 1;
}

/**
 * Get current modifier state
 */
unsigned char keyboard_get_modifiers(void) {
    return modifiers;
}

/**
 * Flush keyboard event queue
 */
void keyboard_flush(void) {
    queue_head = 0;
    queue_tail = 0;
    queue_count = 0;
}
