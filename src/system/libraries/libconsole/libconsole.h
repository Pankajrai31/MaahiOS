/**
 * MaahiOS Console Library - libconsole.h
 * 
 * Description:
 *   Stdout library for console non-interactive .mex apps.
 *   Writes text to a shared memory buffer that the terminal reads
 *   and displays after the app exits.
 * 
 *   Flow:
 *   1. Terminal creates SHM, publishes ID via cell
 *   2. App calls console_init() → attaches to SHM
 *   3. App calls console_print() → writes to SHM buffer
 *   4. App returns → terminal reads buffer, prints to display
 * 
 * Usage:
 *   #include "libconsole.h"
 *   
 *   console_init();
 *   char args[256];
 *   console_get_args(args, 256);
 *   console_print("Hello from console app!\n");
 *   console_print_int(42);
 * 
 * Author: MaahiOS Team
 * Date: March 2026
 */

#ifndef LIBCONSOLE_H
#define LIBCONSOLE_H

#include <stdint.h>

/*=============================================================================
 * SHARED STDOUT BUFFER (shared between terminal and console app)
 *===========================================================================*/

#define CONSOLE_STDOUT_DATA_SIZE   4088   /* Data area (4096 - 8 byte header) */

typedef struct {
    uint32_t write_pos;                       /* Current write offset */
    uint32_t max_size;                        /* Capacity of data[] */
    char     data[CONSOLE_STDOUT_DATA_SIZE];  /* Text output buffer */
} console_stdout_buf_t;

/* Cell key the terminal publishes stdout SHM ID to */
#define CONSOLE_STDOUT_CELL     "system.terminal.stdout_shm"

/* Cell key the terminal publishes command args to */
#define CONSOLE_ARGS_CELL       "system.terminal.args"

/* Cell key the terminal publishes current drive letter to (1 byte: 'C', 'D', ...) */
#define CONSOLE_DRIVE_CELL      "system.terminal.drive"

/*=============================================================================
 * CONSOLE API
 *===========================================================================*/

/**
 * console_init - Initialize console output
 * 
 * Reads the stdout SHM ID from cell, attaches to it.
 * Must be called before any output functions.
 * 
 * Returns: 0 on success, negative on error
 */
int console_init(void);

/**
 * console_get_args - Read command arguments passed by terminal
 * @buf: Buffer to receive args string
 * @max_size: Maximum bytes to read
 * 
 * Returns: Bytes read on success, negative on error
 */
int console_get_args(char *buf, int max_size);

/**
 * console_putchar - Write a single character to stdout
 * @c: Character to write
 */
void console_putchar(char c);

/**
 * console_print - Write a string to stdout
 * @str: Null-terminated string
 */
void console_print(const char *str);

/**
 * console_print_int - Write an integer to stdout
 * @value: Integer value
 */
void console_print_int(int value);

#endif /* LIBCONSOLE_H */
