/**
 * MaahiOS GUI Library - Keyboard Input Implementation
 * 
 * Description:
 *   Reads key events from the keyboard device via the I/O Executive.
 *   Uses libio which routes requests through SHM queues to the
 *   I/O Executive, which is the only process that calls SYS_DEV_READ.
 * 
 *   Flow: kbd_read_event() → libio_dev_read() → SHM → I/O Executive → kernel
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#include "keyboard.h"
#include "../../libio/libio.h"

/*=============================================================================
 * KEYBOARD FUNCTIONS
 *===========================================================================*/

int kbd_read_event(key_event_t *evt) {
    return libio_dev_read(IO_DEV_KEYBOARD, evt, sizeof(key_event_t));
}

int kbd_is_printable(const key_event_t *evt) {
    return (evt->ascii >= 32 && evt->ascii < 127) ? 1 : 0;
}
