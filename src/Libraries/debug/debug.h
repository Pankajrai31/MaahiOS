/**
 * MaahiOS - Debug and Output API
 */

#ifndef MAAHI_DEBUG_H
#define MAAHI_DEBUG_H

/**
 * Print string to serial/debug output
 */
void maahi_print(const char *str);

/**
 * Print single character
 */
void maahi_putchar(char c);

/**
 * Dump resource counts to serial log
 */
void maahi_debug_dump_resources(void);

/**
 * Launch File Manager process
 * @return Process ID or -1 on error
 */
int maahi_launch_file_manager(void);

#endif /* MAAHI_DEBUG_H */
