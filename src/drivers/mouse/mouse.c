/**
 * MaahiOS PS/2 Mouse Driver
 * 
 * Based on OSDev best practices.
 * Registers with Device Manager for unified access.
 */

#include "mouse.h"
#include "../../managers/device/device_manager.h"
#include "../../managers/klog/klog.h"
#include "../../managers/interrupt/idt.h"
#include "../display/display.h"

/* Forward declarations for IRQ setup */
extern void irq_enable_mouse(void);


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
volatile int mouse_x = 0;
volatile int mouse_y = 0;
volatile uint8_t mouse_buttons = 0;
volatile uint8_t mouse_buttons_acc = 0;  /* Accumulated presses since last read */
volatile int irq_total = 0;

/* Screen bounds — set dynamically from display driver in mouse_init() */
static int screen_width = 1024;
static int screen_height = 768;

/* Port I/O */
#include "../../system/libraries/shared/io.h"

/* ============================================
 * Kernel-level Software Cursor
 *
 * Rendered directly in push_packet() which runs
 * inside the IRQ12 handler — zero scheduling
 * delay, the cursor moves the instant the
 * hardware delivers the packet.
 * ============================================ */
#define CURSOR_W 12
#define CURSOR_H 19

static const uint8_t cursor_shape[CURSOR_H][CURSOR_W] = {
    {1,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,0,0,0,0,0,0,0,0,0,0},
    {1,2,1,0,0,0,0,0,0,0,0,0},
    {1,2,2,1,0,0,0,0,0,0,0,0},
    {1,2,2,2,1,0,0,0,0,0,0,0},
    {1,2,2,2,2,1,0,0,0,0,0,0},
    {1,2,2,2,2,2,1,0,0,0,0,0},
    {1,2,2,2,2,2,2,1,0,0,0,0},
    {1,2,2,2,2,2,2,2,1,0,0,0},
    {1,2,2,2,2,2,2,2,2,1,0,0},
    {1,2,2,2,2,2,2,2,2,2,1,0},
    {1,2,2,2,2,2,2,1,1,1,1,0},
    {1,2,2,2,1,2,2,1,0,0,0,0},
    {1,2,2,1,0,1,2,2,1,0,0,0},
    {1,2,1,0,0,1,2,2,1,0,0,0},
    {1,1,0,0,0,0,1,2,2,1,0,0},
    {1,0,0,0,0,0,1,2,2,1,0,0},
    {0,0,0,0,0,0,0,1,2,1,0,0},
    {0,0,0,0,0,0,0,1,1,0,0,0},
};

static uint32_t saved_bg[CURSOR_W * CURSOR_H];
static int saved_cx = -1;
static int saved_cy = -1;
static int cursor_ready = 0;   /* set once gfx is available */
static volatile int cursor_suppress = 0; /* 1 = skip IRQ cursor draw (flip in progress) */

/* Forward declarations for cursor helpers */
static void kcursor_restore(uint32_t *fb, int sw, int sh);
static void kcursor_draw(uint32_t *fb, int sw, int sh, int mx, int my);

void mouse_set_cursor_suppress(int suppress) {
    cursor_suppress = suppress;
}

/**
 * mouse_erase_cursor — remove cursor from HW framebuffer.
 *
 * Restores the saved background pixels at the old cursor position,
 * then marks saved_cx = -1 so no stale restore can occur later.
 * Must be called with IRQs disabled (cli) to prevent IRQ12 race.
 */
void mouse_erase_cursor(void) {
    if (!cursor_ready) return;
    uint32_t *fb = gfx_get_framebuffer();
    if (!fb) return;
    int sw = (int)gfx_get_width();
    int sh = (int)gfx_get_height();
    kcursor_restore(fb, sw, sh);
    saved_cx = -1;
}

static void kcursor_restore(uint32_t *fb, int sw, int sh) {
    if (saved_cx < 0) return;
    int idx = 0;
    for (int r = 0; r < CURSOR_H; r++) {
        int py = saved_cy + r;
        if (py < 0 || py >= sh) { idx += CURSOR_W; continue; }
        for (int c = 0; c < CURSOR_W; c++) {
            int px = saved_cx + c;
            if (px >= 0 && px < sw && cursor_shape[r][c] != 0)
                fb[py * sw + px] = saved_bg[idx];
            idx++;
        }
    }
}

static void kcursor_draw(uint32_t *fb, int sw, int sh, int mx, int my) {
    /* save background */
    int idx = 0;
    for (int r = 0; r < CURSOR_H; r++) {
        int py = my + r;
        for (int c = 0; c < CURSOR_W; c++) {
            int px = mx + c;
            if (py >= 0 && py < sh && px >= 0 && px < sw)
                saved_bg[idx] = fb[py * sw + px];
            else
                saved_bg[idx] = 0;
            idx++;
        }
    }
    saved_cx = mx;
    saved_cy = my;

    /* draw sprite */
    for (int r = 0; r < CURSOR_H; r++) {
        int py = my + r;
        if (py < 0 || py >= sh) continue;
        for (int c = 0; c < CURSOR_W; c++) {
            int px = mx + c;
            if (px < 0 || px >= sw) continue;
            uint8_t v = cursor_shape[r][c];
            if (v == 1)
                fb[py * sw + px] = 0x00000000;
            else if (v == 2)
                fb[py * sw + px] = 0x00FFFFFF;
        }
    }
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

    /* Update current button state + accumulate new presses */
    uint8_t rising = btn & ~mouse_buttons;  /* buttons just pressed */
    mouse_buttons_acc |= rising;
    mouse_buttons = btn;

    /* Update cursor position (1:1 mapping) */
    mouse_x += (int)dx;
    mouse_y += (int)dy;
    
    /* Clamp to screen bounds */
    if (mouse_x < 0) mouse_x = 0;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_x >= screen_width) mouse_x = screen_width - 1;
    if (mouse_y >= screen_height) mouse_y = screen_height - 1;

    /* Draw cursor immediately — runs inside IRQ12 so zero latency.
     * Skip if a framebuffer flip is in progress (cursor_suppress)
     * to avoid drawing on partially-copied HW fb (ghost cursors). */
    if (cursor_ready && !cursor_suppress) {
        uint32_t *fb = gfx_get_framebuffer();
        if (fb) {
            int sw = (int)gfx_get_width();
            int sh = (int)gfx_get_height();
            kcursor_restore(fb, sw, sh);
            kcursor_draw(fb, sw, sh, mouse_x, mouse_y);
        }
    }
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
    /* Disable IRQs so the accumulator read+clear is atomic
     * with respect to mouse IRQ12 (prevents lost clicks). */
    asm volatile("cli");
    state->x = mouse_x;
    state->y = mouse_y;
    state->buttons = mouse_buttons | mouse_buttons_acc;
    mouse_buttons_acc = 0;
    asm volatile("sti");
    
    return sizeof(mouse_state_t);
}

static int mouse_dev_ioctl(int handle, int cmd, void* arg) {
    (void)handle;
    
    switch (cmd) {
        case MOUSE_IOCTL_GET_STATE: {
            if (!arg) return DEV_ERR_INVALID;
            mouse_state_t* state = (mouse_state_t*)arg;
            asm volatile("cli");
            state->x = mouse_x;
            state->y = mouse_y;
            state->buttons = mouse_buttons | mouse_buttons_acc;
            mouse_buttons_acc = 0;
            asm volatile("sti");
            return DEV_OK;
        }
        
        case MOUSE_IOCTL_GET_IRQ_COUNT:
            return irq_total;
        
        case MOUSE_IOCTL_RESET:
            mouse_x = screen_width / 2;
            mouse_y = screen_height / 2;
            mouse_buttons = 0;
            return DEV_OK;

        case MOUSE_IOCTL_CURSOR_HIDE: {
            /* Restore background pixels under cursor, stop drawing */
            if (cursor_ready && saved_cx >= 0) {
                uint32_t *fb = gfx_get_framebuffer();
                if (fb) {
                    int sw = (int)gfx_get_width();
                    int sh = (int)gfx_get_height();
                    kcursor_restore(fb, sw, sh);
                }
            }
            cursor_ready = 0;
            return DEV_OK;
        }

        case MOUSE_IOCTL_CURSOR_SHOW: {
            /* Redraw cursor at current position, re-enable drawing */
            uint32_t *fb = gfx_get_framebuffer();
            if (fb) {
                int sw = (int)gfx_get_width();
                int sh = (int)gfx_get_height();
                kcursor_draw(fb, sw, sh, mouse_x, mouse_y);
            }
            cursor_ready = 1;
            return DEV_OK;
        }
        
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
    KLOG_INFO("MOUSE", "Initializing PS/2 mouse driver");

    /* Query actual screen dimensions from display driver */
    screen_width  = (int)gfx_get_width();
    screen_height = (int)gfx_get_height();
    if (screen_width <= 0)  screen_width  = 1024;
    if (screen_height <= 0) screen_height = 768;
    mouse_x = screen_width / 2;
    mouse_y = screen_height / 2;
    KLOG_INFO("MOUSE", "Screen bounds from display driver");
    
    /* Step 1: Install IRQ handler first */
    KLOG_DEBUG("MOUSE", "Installing IRQ12 handler");
    idt_install_mouse_handler();
    irq_enable_mouse();
    
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

    /* Enable kernel-level cursor rendering.
     * gfx_init() has already run by this point so framebuffer is valid.
     * Do NOT draw initial cursor here — Orbit will repaint the desktop
     * after this, which would make the saved_bg stale and leave a ghost
     * artifact.  The cursor will appear on the first mouse-move IRQ. */
    cursor_ready = 1;
    
    KLOG_INFO("MOUSE", "Driver initialized and registered (sw cursor active)");
    return 0;
}

/**
 * Refresh cursor on HW framebuffer.
 * Called from gfx_flip() AFTER the back buffer has been copied to HW fb.
 * Must be called with IRQs disabled (cli).
 *
 * Resets saved_cx so we don't restore stale old bg, then
 * saves fresh bg from the just-copied HW fb and draws cursor.
 */
void mouse_refresh_cursor(void) {
    if (!cursor_ready) return;
    uint32_t *fb = gfx_get_framebuffer();
    if (!fb) return;
    int sw = (int)gfx_get_width();
    int sh = (int)gfx_get_height();
    /* Mark no old cursor on screen (flip overwrote it) */
    saved_cx = -1;
    /* Save bg at current position from fresh HW fb, draw sprite */
    kcursor_draw(fb, sw, sh, mouse_x, mouse_y);
}
