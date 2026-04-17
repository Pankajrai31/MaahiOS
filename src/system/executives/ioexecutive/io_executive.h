/**
 * MaahiOS I/O Executive Header
 * 
 * Description:
 *   Defines operations and payload structures for the I/O Executive.
 *   The I/O Executive is the single gateway for all device input:
 *   keyboard, mouse (future), and other input devices.
 * 
 *   Applications never call SYS_DEV_READ directly. Instead they use
 *   libio, which routes requests through this executive via SHM queues.
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#ifndef IO_EXECUTIVE_H
#define IO_EXECUTIVE_H

#include <stdint.h>
#include "../common/executive_common.h"

/*=============================================================================
 * I/O EXECUTIVE OPERATIONS
 *===========================================================================*/

/** Read data from a device (keyboard, mouse, etc.) */
#define IO_OP_DEV_READ          (EXEC_OP_CUSTOM_BASE + 0)

/** Open a device (future use) */
#define IO_OP_DEV_OPEN          (EXEC_OP_CUSTOM_BASE + 1)

/*=============================================================================
 * DEVICE IDs (must match device_manager.h)
 *===========================================================================*/

#define IO_DEV_KEYBOARD         2
#define IO_DEV_MOUSE            1   /* Must match DEV_MOUSE in device_manager.h */

/*=============================================================================
 * REQUEST / RESPONSE PAYLOADS
 *===========================================================================*/

/**
 * IO_OP_DEV_READ request payload
 * Client specifies which device and how many bytes to read.
 */
typedef struct {
    uint32_t device_id;     /* IO_DEV_KEYBOARD, IO_DEV_MOUSE, etc. */
    uint32_t max_size;      /* Maximum bytes to read */
} io_dev_read_req_t;

/**
 * IO_OP_DEV_READ response payload
 * Executive returns the raw device data.
 * resp.result = bytes read (>0), 0 (no data), or negative (error)
 */
typedef struct {
    uint32_t bytes_read;    /* Actual bytes read from device */
    uint8_t  data[64];      /* Raw device data (key_event_t, mouse_event_t, etc.) */
} io_dev_read_resp_t;

/*=============================================================================
 * KEYBOARD RING BUFFER (shared memory, lock-free SPSC)
 *
 * Instead of per-event SHM request/response round-trips (which inherit
 * scheduler latency), the I/O Executive continuously polls the keyboard
 * and writes events into this ring buffer in shared memory.
 * libio reads directly from this buffer — zero syscalls, zero IPC.
 *
 * This is the same pattern as Linux evdev and Windows raw input:
 * the input subsystem writes events to a shared buffer that apps read.
 *===========================================================================*/

#define IO_KBD_RING_CAPACITY    32
#define IO_KBD_RING_ENTRY_SIZE  8   /* Enough for key_event_t (4 bytes) + pad */

/**
 * Single-producer single-consumer ring buffer for keyboard events.
 * Produced by I/O Executive, consumed by libio in user apps.
 * Lives in shared memory; discovered via cell "system.io.keyboard.ring_shm".
 *
 * focused_pid: WM Executive writes the PID of the focused window here.
 * Apps check this before consuming — only the focused app reads events.
 * This prevents keyboard event theft between competing apps.
 */
typedef struct {
    volatile uint32_t head;         /* Write index (I/O Executive only) */
    volatile uint32_t tail;         /* Read index (consumer only) */
    uint32_t capacity;              /* IO_KBD_RING_CAPACITY */
    uint32_t entry_size;            /* IO_KBD_RING_ENTRY_SIZE */
    volatile int32_t focused_pid;   /* PID that may consume (WM writes) */
    uint8_t data[IO_KBD_RING_CAPACITY * IO_KBD_RING_ENTRY_SIZE];
} io_kbd_ring_t;

/*=============================================================================
 * MOUSE STATE SLOT (shared memory, lock-free single-value)
 *
 * The mouse is state-based (current position + buttons), not event-based.
 * The I/O Executive continuously polls the mouse device and writes the
 * latest state to this slot in shared memory. libio reads directly from
 * here — zero syscalls, zero IPC.  Much simpler than a ring buffer.
 *
 * Discovered via cell "system.io.mouse.state_shm".
 *===========================================================================*/

typedef struct {
    volatile int      x;         /* Current X position */
    volatile int      y;         /* Current Y position */
    volatile uint8_t  buttons;   /* Button state (MOUSE_LEFT, etc.) */
    volatile uint32_t seq;       /* Sequence counter (writer increments) */
} io_mouse_state_t;

#endif /* IO_EXECUTIVE_H */
