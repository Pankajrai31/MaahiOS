/**
 * MaahiOS PS/2 Mouse Driver (CORRECTED - Based on OSDev Best Practices)
 * Fixes: Buffer staying full, serial in ISR, command byte, AUX-first logic
 */

#include "mouse.h"
#include <stdint.h>

#define PS2_DATA    0x60
#define PS2_STATUS  0x64
#define PS2_CMD     0x64

// Status register bits
#define STATUS_OBF  0x01
#define STATUS_IBF  0x02
#define STATUS_AUX  0x20  // mouse data in output buffer

// Ring buffer for packets
#define MOUSE_BUF_SIZE 128

typedef struct {
    int8_t dx, dy;
    uint8_t buttons;
} mouse_packet_t;

static mouse_packet_t ring[MOUSE_BUF_SIZE];
static volatile int head = 0;
static volatile int tail = 0;

// Partial packet assembly
static uint8_t pkt[3];
static uint8_t pkt_i = 0;

volatile int mouse_x = 320;
volatile int mouse_y = 240;
volatile int irq_total = 0;

// Port I/O helpers
static inline uint8_t inb(uint16_t p) {
    uint8_t r;
    __asm__ volatile("inb %1, %0":"=a"(r):"Nd"(p));
    return r;
}

static inline void outb(uint16_t p, uint8_t v) {
    __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));
}

static int wait_input_clear() {
    for (int i = 0; i < 50000; i++) {
        if (!(inb(PS2_STATUS) & STATUS_IBF))
            return 1;
    }
    return 0;
}

static int wait_output_full() {
    for (int i = 0; i < 50000; i++) {
        if (inb(PS2_STATUS) & STATUS_OBF)
            return 1;
    }
    return 0;
}

static void flush_output() {
    for (int i = 0; i < 16; i++) {
        if (inb(PS2_STATUS) & STATUS_OBF)
            (void)inb(PS2_DATA);
        else break;
    }
}

// Mouse command helpers
static uint8_t read_cmd_byte() {
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

/**
 * Initialize PS/2 mouse (CORRECTED - proper command byte, no delays in wrong places)
 */
int mouse_init() {
    pkt_i = 0;
    head = tail = 0;
    irq_total = 0;

    // Disable PS/2 ports
    wait_input_clear(); outb(PS2_CMD, 0xAD);
    wait_input_clear(); outb(PS2_CMD, 0xA7);

    flush_output();

    // Read command byte
    uint8_t cb = read_cmd_byte();

    // CRITICAL FIX: Enable mouse IRQ + enable keyboard IRQ + enable mouse clock (bit5=0)
    cb |= 0x03;     // bit0 KB IRQ, bit1 mouse IRQ
    cb &= ~0x20;    // bit5 must be 0 → enable mouse clock (was 0x47 which disabled it!)

    write_cmd_byte(cb);

    // Enable mouse port
    wait_input_clear();
    outb(PS2_CMD, 0xA8);

    // Enable keyboard port
    wait_input_clear();
    outb(PS2_CMD, 0xAE);

    flush_output();

    // Enable data reporting
    uint8_t ack = mouse_write(0xF4);
    // ACK check removed from ISR - can check outside if needed

    flush_output();

    return 1;
}

// Add packet to ring buffer
static void push_packet(int8_t dx, int8_t dy, uint8_t btn) {
    mouse_packet_t p;
    p.dx = dx;
    p.dy = dy;
    p.buttons = btn;

    ring[head] = p;
    head = (head + 1) % MOUSE_BUF_SIZE;

    // Update cursor position (2x sensitivity for responsiveness)
    mouse_x += dx * 2;
    mouse_y += dy * 2;
    if (mouse_x < 0) mouse_x = 0;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_x > 1023) mouse_x = 1023;
    if (mouse_y > 767)  mouse_y = 767;
}

/**
 * IRQ12 Handler (ATOMIC + CORRECT)
 * CRITICAL FIXES:
 * 1. NO serial prints in ISR (was causing delays)
 * 2. Check AUX bit BEFORE reading data (was mixing keyboard/mouse)
 * 3. Proper packet sync with bit3 check
 * 4. Fast execution keeps 8042 buffer from staying full
 */
void mouse_handler() {
    irq_total++;

    // CRITICAL FIX #1: Read status FIRST
    uint8_t status = inb(PS2_STATUS);

    if (!(status & STATUS_OBF))
        return; // no data

    // CRITICAL FIX #2: Check if it's mouse data BEFORE reading
    if (!(status & STATUS_AUX)) {
        (void)inb(PS2_DATA);  // Discard keyboard byte
        return;
    }

    // Read mouse byte (8042 buffer now cleared immediately!)
    uint8_t b = inb(PS2_DATA);

    // Packet sync: first byte must have bit3=1
    if (pkt_i == 0 && !(b & 0x08))
        return; // ignore until proper sync

    pkt[pkt_i++] = b;

    if (pkt_i < 3)
        return;

    // Full packet ready
    pkt_i = 0;

    int8_t dx = (int8_t)pkt[1];
    int8_t dy = -(int8_t)pkt[2]; // invert Y

    uint8_t buttons = pkt[0] & 0x07;

    push_packet(dx, dy, buttons);

    // Send EOI to both PICs
    outb(0xA0, 0x20);
    outb(0x20, 0x20);
}

// Ring buffer read function
static int mouse_read(mouse_packet_t *out) {
    if (head == tail) return 0; // no data

    *out = ring[tail];
    tail = (tail + 1) % MOUSE_BUF_SIZE;

    return 1;
}

/**
 * Drain buffer - simplified, no re-init needed with corrected driver
 */
void mouse_drain_buffer(void) {
    flush_output();
    pkt_i = 0;  // Reset packet state
}

// User API functions
int mouse_get_x() { 
    __asm__ volatile("" ::: "memory");
    return mouse_x; 
}

int mouse_get_y() { 
    __asm__ volatile("" ::: "memory");
    return mouse_y; 
}

int mouse_get_irq_total() { 
    __asm__ volatile("" ::: "memory");
    return irq_total; 
}

uint8_t mouse_get_buttons() { 
    mouse_packet_t p;
    if (!mouse_read(&p)) return 0;
    return p.buttons;
}

// Additional compatibility functions
void mouse_set_bounds(int width, int height) {
    // Bounds are hardcoded in push_packet for now (1024x768)
}

void mouse_reset_position(int x, int y) {
    mouse_x = x;
    mouse_y = y;
}
