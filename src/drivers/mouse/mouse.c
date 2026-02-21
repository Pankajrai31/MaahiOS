/**
 * MaahiOS PS/2 Mouse Driver
 * 
 * Based on OSDev best practices.
 * Registers with Device Manager for unified access.
 */

#include "mouse.h"
#include "../../managers/device/device_manager.h"

/* ============================================
 * PS/2 Controller Ports
 * ============================================ */
#define PS2_DATA    0x60
#define PS2_STATUS  0x64
#define PS2_CMD     0x64

/* Status register bits */
#define STATUS_OBF  0x01    /* Output buffer full */
#define STATUS_IBF  0x02    /* Input buffer full */
#define STATUS_AUX  0x20    /* Mouse data in output buffer */

/* ============================================
 * Ring Buffer for Mouse Packets
 * ============================================ */
#define MOUSE_BUF_SIZE 128

typedef struct {
    int8_t dx, dy;
    uint8_t buttons;
} mouse_packet_t;

static mouse_packet_t ring[MOUSE_BUF_SIZE];
static volatile int head = 0;
static volatile int tail = 0;

/* Partial packet assembly */
static uint8_t pkt[3];
static uint8_t pkt_i = 0;

/* Current mouse state (updated each IRQ) */
volatile int mouse_x = 512;
volatile int mouse_y = 384;
volatile uint8_t mouse_buttons = 0;
volatile int irq_total = 0;

/* Screen bounds */
static int screen_width = 1024;
static int screen_height = 768;

/* ============================================
 * Port I/O Helpers
 * ============================================ */
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* ============================================
 * PS/2 Controller Helpers
 * ============================================ */
static int wait_input_clear(void) {
    for (int i = 0; i < 50000; i++) {
        if (!(inb(PS2_STATUS) & STATUS_IBF))
            return 1;
    }
    return 0;
}

static int wait_output_full(void) {
    for (int i = 0; i < 50000; i++) {
        if (inb(PS2_STATUS) & STATUS_OBF)
            return 1;
    }
    return 0;
}

static void flush_output(void) {
    for (int i = 0; i < 16; i++) {
        if (inb(PS2_STATUS) & STATUS_OBF)
            (void)inb(PS2_DATA);
        else
            break;
    }
}

static uint8_t read_cmd_byte(void) {
    wait_input_clear();
    outb(PS2_CMD, 0x20);
    wait_output_full();
    return inb(PS2_DATA);
}

static void write_cmd_byte(uint8_t b) {
    wait_input_clear();
    outb(PS2_CMD, 0x60);
    wait_input_clear();
    outb(PS2_DATA, b);
}

static uint8_t mouse_write(uint8_t b) {
    wait_input_clear();
    outb(PS2_CMD, 0xD4);
    wait_input_clear();
    outb(PS2_DATA, b);
    wait_output_full();
    return inb(PS2_DATA);
}

/* ============================================
 * Packet Processing
 * ============================================ */
static void push_packet(int8_t dx, int8_t dy, uint8_t btn) {
    mouse_packet_t p;
    p.dx = dx;
    p.dy = dy;
    p.buttons = btn;

    ring[head] = p;
    head = (head + 1) % MOUSE_BUF_SIZE;

    /* Update current button state */
    mouse_buttons = btn;

    /* Update cursor position */
    mouse_x += dx;
    mouse_y += dy;
    
    /* Clamp to screen bounds */
    if (mouse_x < 0) mouse_x = 0;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_x >= screen_width) mouse_x = screen_width - 1;
    if (mouse_y >= screen_height) mouse_y = screen_height - 1;

    /* Update hardware cursor (if available) */
    extern void bga_cursor_set_position(int x, int y);
    bga_cursor_set_position(mouse_x, mouse_y);
}

/* ============================================
 * Device Manager Operations
 * ============================================ */
static int mouse_dev_open(int flags) {
    (void)flags;
    return 0;  /* Always succeeds, single handle */
}

static int mouse_dev_close(int handle) {
    (void)handle;
    return 0;
}

static int mouse_dev_read(int handle, void* buffer, size_t size) {
    (void)handle;
    
    if (!buffer || size < sizeof(mouse_state_t)) {
        return DEV_ERR_INVALID;
    }
    
    mouse_state_t* state = (mouse_state_t*)buffer;
    state->x = mouse_x;
    state->y = mouse_y;
    state->buttons = mouse_buttons;
    
    return sizeof(mouse_state_t);
}

static int mouse_dev_ioctl(int handle, int cmd, void* arg) {
    (void)handle;
    
    switch (cmd) {
        case MOUSE_IOCTL_GET_STATE: {
            if (!arg) return DEV_ERR_INVALID;
            mouse_state_t* state = (mouse_state_t*)arg;
            state->x = mouse_x;
            state->y = mouse_y;
            state->buttons = mouse_buttons;
            return DEV_OK;
        }
        
        case MOUSE_IOCTL_GET_IRQ_COUNT:
            return irq_total;
        
        case MOUSE_IOCTL_RESET:
            mouse_x = screen_width / 2;
            mouse_y = screen_height / 2;
            mouse_buttons = 0;
            return DEV_OK;
        
        default:
            return DEV_ERR_INVALID;
    }
}

static int mouse_dev_poll(int handle) {
    (void)handle;
    /* Return 1 if there's data in the ring buffer */
    return (head != tail) ? 1 : 0;
}

/* Device operations table */
static device_ops_t mouse_ops = {
    .open  = mouse_dev_open,
    .close = mouse_dev_close,
    .read  = mouse_dev_read,
    .write = (void*)0,  /* Mouse doesn't support write */
    .ioctl = mouse_dev_ioctl,
    .poll  = mouse_dev_poll
};

/* ============================================
 * IRQ12 Handler
 * ============================================ */
void mouse_handler(void) {
    irq_total++;

    /* Read status first */
    uint8_t status = inb(PS2_STATUS);

    if (!(status & STATUS_OBF))
        return;

    /* Check if it's mouse data */
    if (!(status & STATUS_AUX)) {
        (void)inb(PS2_DATA);  /* Discard keyboard byte */
        return;
    }

    /* Read mouse byte */
    uint8_t b = inb(PS2_DATA);

    /* Packet sync: first byte must have bit3=1 */
    if (pkt_i == 0 && !(b & 0x08)) {
        flush_output();
        return;
    }

    pkt[pkt_i++] = b;

    if (pkt_i < 3)
        return;

    /* Full packet ready */
    pkt_i = 0;

    int8_t dx = (int8_t)pkt[1];
    int8_t dy = -(int8_t)pkt[2];  /* Invert Y */
    uint8_t buttons = pkt[0] & 0x07;

    push_packet(dx, dy, buttons);

    /* Send EOI to both PICs */
    outb(0xA0, 0x20);
    outb(0x20, 0x20);
}

/* ============================================
 * Driver Initialization
 * ============================================ */
int mouse_init(void) {
    pkt_i = 0;
    head = tail = 0;
    irq_total = 0;

    /* Disable PS/2 ports */
    wait_input_clear();
    outb(PS2_CMD, 0xAD);
    wait_input_clear();
    outb(PS2_CMD, 0xA7);

    flush_output();

    /* Configure command byte */
    uint8_t cb = read_cmd_byte();
    cb |= 0x03;     /* Enable KB + mouse IRQ */
    cb &= ~0x20;    /* Enable mouse clock */
    write_cmd_byte(cb);

    /* Enable mouse port */
    wait_input_clear();
    outb(PS2_CMD, 0xA8);

    /* Enable keyboard port */
    wait_input_clear();
    outb(PS2_CMD, 0xAE);

    flush_output();

    /* Enable data reporting */
    mouse_write(0xF4);

    flush_output();

    /* Register with Device Manager */
    register_device(DEV_MOUSE, "mouse", &mouse_ops);

    return 1;
}
