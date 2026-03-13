/**
 * MaahiOS Console Library - libconsole.c
 * 
 * Description:
 *   Stdout implementation for console non-interactive .mex apps.
 *   Writes to a shared memory buffer published by the terminal.
 * 
 * Author: MaahiOS Team
 * Date: March 2026
 */

#include "libconsole.h"
#include "../libcell/libcell.h"
#include "../core/syscall_helpers.h"

/*=============================================================================
 * STATE
 *===========================================================================*/

static console_stdout_buf_t *g_stdout = 0;  /* Pointer to SHM stdout buffer */
static int g_shm_id = -1;                   /* SHM ID we attached to */

/*=============================================================================
 * INITIALIZATION
 *===========================================================================*/

int console_init(void) {
    /* Read stdout SHM ID from cell published by terminal */
    int shm_id = 0;
    int rc = libcell_read(CONSOLE_STDOUT_CELL, &shm_id, sizeof(int));
    if (rc < 0 || shm_id <= 0) {
        return -1;
    }

    g_shm_id = shm_id;

    /* Attach to the SHM region */
    g_stdout = (console_stdout_buf_t *)syscall2(SYS_SHM_ATTACH, shm_id, 0);
    if (!g_stdout) {
        return -2;
    }

    return 0;
}

/*=============================================================================
 * ARGS
 *===========================================================================*/

int console_get_args(char *buf, int max_size) {
    return libcell_read(CONSOLE_ARGS_CELL, buf, (uint32_t)max_size);
}

/*=============================================================================
 * OUTPUT
 *===========================================================================*/

void console_putchar(char c) {
    if (!g_stdout) return;
    if (g_stdout->write_pos < g_stdout->max_size) {
        g_stdout->data[g_stdout->write_pos] = c;
        g_stdout->write_pos++;
    }
}

void console_print(const char *str) {
    if (!g_stdout) return;
    while (*str) {
        console_putchar(*str);
        str++;
    }
}

void console_print_int(int value) {
    if (!g_stdout) return;

    if (value < 0) {
        console_putchar('-');
        value = -value;
    }

    if (value == 0) {
        console_putchar('0');
        return;
    }

    /* Convert digits in reverse */
    char buf[12];
    int i = 0;
    while (value > 0 && i < 11) {
        buf[i++] = '0' + (char)(value % 10);
        value /= 10;
    }

    /* Print in correct order */
    while (i > 0) {
        console_putchar(buf[--i]);
    }
}
