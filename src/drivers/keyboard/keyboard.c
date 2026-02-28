/**
 * MaahiOS PS/2 Keyboard Driver
 * 
 * Provides keyboard input with scancode translation and event queue.
 * Registers with Device Manager for unified access.
 */

#include "keyboard.h"
#include "../../managers/device/device_manager.h"
#include "../../managers/klog/klog.h"

/* ============================================
 * Keyboard Ports
 * ============================================ */
#define KBD_DATA_PORT    0x60
#define KBD_STATUS_PORT  0x64
#define KBD_CMD_PORT     0x64

/* ============================================
 * Event Queue
 * ============================================ */
#define EVENT_QUEUE_SIZE 32
static key_event_t event_queue[EVENT_QUEUE_SIZE];
static volatile int queue_head = 0;
static volatile int queue_tail = 0;
static volatile int queue_count = 0;

/* ============================================
 * Modifier State
 * ============================================ */
static volatile uint8_t modifiers = 0;
static volatile int keyboard_initialized = 0;

/* ============================================
 * Scancode Tables (US Layout)
 * ============================================ */
static const uint8_t scancode_to_ascii[128] = {
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

static const uint8_t scancode_to_ascii_shift[128] = {
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

/* Port I/O */
#include "../../system/libraries/shared/io.h"

/* ============================================
 * Queue Management
 * ============================================ */
static void queue_add_event(uint8_t type, uint8_t keycode, 
                            uint8_t ascii, uint8_t scancode) {
    if (queue_count >= EVENT_QUEUE_SIZE) {
        return;  /* Queue full, drop event */
    }
    
    event_queue[queue_tail].type = type;
    event_queue[queue_tail].keycode = keycode;
    event_queue[queue_tail].ascii = ascii;
    event_queue[queue_tail].modifiers = modifiers;
    event_queue[queue_tail].scancode = scancode;
    
    queue_tail = (queue_tail + 1) % EVENT_QUEUE_SIZE;
    queue_count++;
}

/* ============================================
 * Device Manager Operations
 * ============================================ */
static int keyboard_dev_open(int flags) {
    (void)flags;
    return 0;
}

static int keyboard_dev_close(int handle) {
    (void)handle;
    return 0;
}

static int keyboard_dev_read(int handle, void* buffer, size_t size) {
    (void)handle;
    
    if (!buffer || size < sizeof(key_event_t)) {
        return DEV_ERR_INVALID;
    }
    
    if (queue_count == 0) {
        return 0;  /* No data available */
    }
    
    key_event_t* event = (key_event_t*)buffer;
    *event = event_queue[queue_head];
    queue_head = (queue_head + 1) % EVENT_QUEUE_SIZE;
    queue_count--;
    
    return sizeof(key_event_t);
}

static int keyboard_dev_ioctl(int handle, int cmd, void* arg) {
    (void)handle;
    (void)arg;
    
    switch (cmd) {
        case KB_IOCTL_GET_SCANCODE:
            /* Return last scancode */
            return 0;  /* TODO */
        
        case KB_IOCTL_SET_LEDS:
            /* TODO: Set keyboard LEDs */
            return DEV_OK;
        
        default:
            return DEV_ERR_INVALID;
    }
}

static int keyboard_dev_poll(int handle) {
    (void)handle;
    return (queue_count > 0) ? 1 : 0;
}

/* Device operations table */
static device_ops_t keyboard_ops = {
    .open  = keyboard_dev_open,
    .close = keyboard_dev_close,
    .read  = keyboard_dev_read,
    .write = (void*)0,
    .ioctl = keyboard_dev_ioctl,
    .poll  = keyboard_dev_poll
};

/* ============================================
 * IRQ1 Handler
 * ============================================ */
void keyboard_irq_handler(void) {
    /* Read scan code */
    uint8_t scancode = inb(KBD_DATA_PORT);
    
    /* Check if key release (bit 7 set) */
    int released = scancode & 0x80;
    uint8_t code = scancode & 0x7F;
    
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
        modifiers ^= MOD_CAPS;
        return;
    }
    
    /* Debug: Ctrl+Alt+Shift = Dump klog */
    if ((modifiers & MOD_CTRL) && (modifiers & MOD_ALT) && (modifiers & MOD_SHIFT)) {
        if (!released && code != SC_CTRL && code != SC_ALT && 
            code != SC_LSHIFT && code != SC_RSHIFT) {
            KLOG_INFO("KEYBOARD", "Manual dump triggered");
            extern void klog_dump(void);
            klog_dump();
            return;
        }
    }
    
    /* Get ASCII character */
    uint8_t ascii;
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

/* ============================================
 * Driver Initialization
 * ============================================ */
int keyboard_init(void) {
    /* Clear event queue */
    queue_head = 0;
    queue_tail = 0;
    queue_count = 0;
    modifiers = 0;
    
    /* Enable keyboard IRQ (IRQ1) */
    uint8_t mask = inb(0x21);
    mask &= ~0x02;  /* Clear bit 1 to enable IRQ1 */
    outb(0x21, mask);
    
    keyboard_initialized = 1;
    
    /* Register with Device Manager */
    register_device(DEV_KEYBOARD, "keyboard", &keyboard_ops);
    
    KLOG_INFO("KEYBOARD", "Initialized and registered");
    return 0;  /* Success */
}
