/**
 * MaahiOS - Process Management API
 */

#ifndef MAAHI_PROCESS_H
#define MAAHI_PROCESS_H

/**
 * Create a new process
 * @param entry_point Address of process entry point
 * @return PID of new process, or -1 on error
 */
int maahi_create_process(unsigned int entry_point);

/**
 * Get UIManager module address (for sysman)
 * @return Address of UIManager entry point
 */
unsigned int maahi_get_uimanager_address(void);

/**
 * Get Orbit module address (for sysman)
 * @return Address of Orbit entry point
 */
unsigned int maahi_get_orbit_address(void);

#endif /* MAAHI_PROCESS_H */
