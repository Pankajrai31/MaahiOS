/**
 * MaahiOS Shared Time Library - shared_time.h
 * 
 * Description:
 *   Shared time structure for user-space access without syscalls.
 *   Time Manager updates this struct in SHM every second.
 *   User-space libraries read directly from SHM.
 * 
 * Architecture:
 *   This is a "Shared Library" - it reads/writes SHM directly.
 *   Both executives and applications can use it.
 *   Does NOT call any executive (no SHM queue involved).
 * 
 * Usage:
 *   int shm_id = time_get_shared_shm_id();  // Get from syscall or cell
 *   shared_time_init(shm_id);               // Attach to SHM
 *   uint32_t unix = shared_time_get_unix(); // Read time (no syscall!)
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#ifndef SHARED_TIME_H
#define SHARED_TIME_H

#include <stdint.h>

/*=============================================================================
 * SHARED TIME STRUCTURE
 * 
 * Updated by Time Manager in kernel, read by user-space
 *===========================================================================*/

typedef struct {
    volatile uint32_t lock;         /* Spinlock (for future use) */
    
    /* Time values */
    uint32_t unix_time;             /* Unix timestamp (seconds since epoch) */
    uint64_t ticks;                 /* PIT ticks since boot */
    
    /* Datetime breakdown */
    uint16_t year;                  /* Full year (e.g., 2026) */
    uint8_t  month;                 /* 1-12 */
    uint8_t  day;                   /* 1-31 */
    uint8_t  hour;                  /* 0-23 */
    uint8_t  minute;                /* 0-59 */
    uint8_t  second;                /* 0-59 */
    uint8_t  weekday;               /* 1-7 (Sunday = 1) */
    
    /* Sequence counter for lock-free reads */
    volatile uint32_t update_seq;   /* Incremented before/after updates */
    
    /* SHM identification */
    int32_t  shm_id;                /* SHM ID of this structure */
} shared_time_t;

/*=============================================================================
 * LIBRARY API
 *===========================================================================*/

/**
 * shared_time_init - Initialize shared time access
 * @shm_id: SHM ID of shared time region
 * @return: 0 on success, negative on error
 */
int shared_time_init(int shm_id);

/**
 * shared_time_get_unix - Get Unix timestamp
 * @return: Seconds since epoch, or 0 if not initialized
 */
uint32_t shared_time_get_unix(void);

/**
 * shared_time_get_ticks - Get tick count
 * @return: Ticks since boot, or 0 if not initialized
 */
uint64_t shared_time_get_ticks(void);

/**
 * shared_time_get_datetime - Get datetime breakdown
 * @hour, @minute, @second, @month, @day: Output pointers
 * @return: 0 on success, negative on error
 */
int shared_time_get_datetime(uint8_t *hour, uint8_t *minute, uint8_t *second,
                              uint8_t *month, uint8_t *day);

/**
 * shared_time_get_ptr - Get direct pointer to shared time struct
 * 
 * For advanced use cases where caller wants raw access.
 * Caller must handle update_seq for consistency.
 * 
 * @return: Pointer to shared_time_t, or NULL if not initialized
 */
shared_time_t* shared_time_get_ptr(void);

#endif /* SHARED_TIME_H */
