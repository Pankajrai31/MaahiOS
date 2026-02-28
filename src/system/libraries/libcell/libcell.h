/**
 * MaahiOS Cell Library - libcell.h
 * 
 * Description:
 *   User library for cell registry access.
 *   Auto-initializes on first call. Falls back to direct kernel
 *   cell syscalls if Cell Executive is not yet running.
 * 
 * Usage:
 *   #include <libcell.h>
 *   
 *   libcell_write_int("app.settings.volume", 80);  // just call it
 *   int32_t vol;
 *   libcell_read_int("app.settings.volume", &vol);
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#ifndef LIBCELL_H
#define LIBCELL_H

#include <stdint.h>
#include "../../executives/common/executive_common.h"
#include "../../executives/cellexecutive/cell_executive.h"

/*=============================================================================
 * LIBRARY INITIALIZATION
 *===========================================================================*/

/**
 * libcell_init - Explicitly initialize cell library (optional)
 * 
 * Auto-called on first use of any libcell function.
 * Falls back to direct kernel syscalls if executive not ready.
 * 
 * Returns: 0 on success, negative if executive not available
 */
int libcell_init(void);

/**
 * libcell_shutdown - Cleanup cell library
 * 
 * Detaches from SHM queues.
 */
void libcell_shutdown(void);

/*=============================================================================
 * CELL OPERATIONS
 *===========================================================================*/

/**
 * libcell_register - Register a new cell
 * @name: Cell name (hierarchical, e.g., "app.settings.volume")
 * @type: Cell type (CELL_TYPE_*)
 * @flags: Cell flags (CELL_FLAG_*)
 * 
 * Returns: Cell ID on success, negative on error
 */
int libcell_register(const char *name, cell_type_t type, uint32_t flags);

/**
 * libcell_write - Write data to a cell
 * @name: Cell name
 * @data: Data to write
 * @size: Size of data in bytes
 * 
 * Returns: 0 on success, negative on error
 */
int libcell_write(const char *name, const void *data, uint32_t size);

/**
 * libcell_read - Read data from a cell
 * @name: Cell name
 * @buffer: Buffer to receive data
 * @max_size: Maximum bytes to read
 * 
 * Returns: Bytes read on success, negative on error
 */
int libcell_read(const char *name, void *buffer, uint32_t max_size);

/**
 * libcell_write_int - Write an integer to a cell
 * @name: Cell name
 * @value: Integer value
 * 
 * Returns: 0 on success, negative on error
 */
int libcell_write_int(const char *name, int32_t value);

/**
 * libcell_read_int - Read an integer from a cell
 * @name: Cell name
 * @value: Output pointer for value
 * 
 * Returns: 0 on success, negative on error
 */
int libcell_read_int(const char *name, int32_t *value);

/**
 * libcell_delete - Delete a cell
 * @name: Cell name
 * 
 * Returns: 0 on success, negative on error
 */
int libcell_delete(const char *name);

/**
 * libcell_exists - Check if a cell exists
 * @name: Cell name
 * 
 * Returns: 1 if exists, 0 if not, negative on error
 */
int libcell_exists(const char *name);

/**
 * libcell_lookup - Look up a cell by name
 * @name: Cell name
 * 
 * Returns: Cell ID if found, negative on error
 */
int libcell_lookup(const char *name);

/**
 * libcell_get_info - Get cell information
 * @name: Cell name
 * @info: Output structure
 * 
 * Returns: 0 on success, negative on error
 */
int libcell_get_info(const char *name, cell_info_t *info);

#endif /* LIBCELL_H */
