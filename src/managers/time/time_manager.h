/**
 * MaahiOS Time Manager
 * 
 * Provides system-wide time tracking:
 * - Reads RTC once at boot
 * - Increments using PIT ticks for runtime
 * - Syscalls for Ring 3 access
 */

#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <stdint.h>

/* ============================================
 * Time Structures
 * ============================================ */

/* Compact datetime for syscall responses */
typedef struct {
    uint16_t year;       /* Full year (e.g., 2026) */
    uint8_t  month;      /* 1-12 */
    uint8_t  day;        /* 1-31 */
    uint8_t  hour;       /* 0-23 */
    uint8_t  minute;     /* 0-59 */
    uint8_t  second;     /* 0-59 */
    uint8_t  weekday;    /* 1-7 (Sunday = 1) */
} sys_datetime_t;

/* Uptime structure */
typedef struct {
    uint32_t days;
    uint8_t  hours;
    uint8_t  minutes;
    uint8_t  seconds;
    uint16_t millis;     /* Milliseconds (0-999) */
    uint64_t total_ticks;
} sys_uptime_t;

/* ============================================
 * Manager API (Kernel)
 * ============================================ */

/**
 * Initialize time manager.
 * Reads boot time from RTC.
 * Call AFTER rtc_init() and pit_init().
 * @return 0 on success, negative on error
 */
int time_manager_init(void);

/**
 * Check if time system is initialized.
 * @return 1 if initialized, 0 if not
 */
int kernel_time_is_initialized(void);

/**
 * Get current date/time.
 * @param dt Output datetime structure
 * @return 0 on success, negative on error
 */
int kernel_time_get_datetime(sys_datetime_t *dt);

/**
 * Get current Unix timestamp.
 * @return Seconds since Jan 1, 1970
 */
uint32_t kernel_time_get_unix(void);

/**
 * Get system uptime.
 * @param up Output uptime structure
 * @return 0 on success, negative on error
 */
int kernel_time_get_uptime(sys_uptime_t *up);

/**
 * Get raw tick count from PIT.
 * @return Ticks since boot
 */
uint64_t kernel_time_get_ticks(void);

/**
 * Get tick frequency (ticks per second).
 * @return Ticks per second (e.g., 50)
 */
uint32_t kernel_time_get_tick_frequency(void);

/* ============================================
 * Shared Time API (for User-Space)
 * ============================================ */

/**
 * Timer tick callback.
 * Called from PIT IRQ handler.
 * Updates shared time structure once per second.
 */
void time_manager_tick(void);

/**
 * Get SHM ID for shared time structure.
 * User-space can attach to read time without syscalls.
 * @return SHM ID, or -1 if not available
 */
int kernel_time_get_shared_shm_id(void);

#endif /* TIME_MANAGER_H */
