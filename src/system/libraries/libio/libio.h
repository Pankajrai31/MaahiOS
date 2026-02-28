/**
 * MaahiOS I/O Library - libio.h
 * 
 * Description:
 *   User-space library for device input via the I/O Executive.
 *   Auto-initializes on first call (discovers SHM queues via cells).
 * 
 *   This is the ONLY way user-mode code should read from input devices.
 *   Libraries (libgui/keyboard) call these functions; apps use libgui.
 * 
 * Usage:
 *   #include "libio.h"
 *   
 *   uint8_t buf[8];
 *   int n = libio_dev_read(IO_DEV_KEYBOARD, buf, sizeof(buf));
 *   if (n > 0) { ... }
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#ifndef LIBIO_H
#define LIBIO_H

#include <stdint.h>
#include "../../executives/ioexecutive/io_executive.h"

/*=============================================================================
 * DEVICE READ
 *===========================================================================*/

/**
 * libio_dev_read - Read data from a device via the I/O Executive
 * @device_id: Device to read from (IO_DEV_KEYBOARD, IO_DEV_MOUSE, etc.)
 * @buffer: Output buffer for device data
 * @max_size: Maximum bytes to read
 * 
 * Non-blocking. Returns same semantics as SYS_DEV_READ:
 *   >0: Number of bytes read (event available)
 *    0: No data pending
 *   <0: Error
 */
int libio_dev_read(uint32_t device_id, void *buffer, uint32_t max_size);

#endif /* LIBIO_H */
