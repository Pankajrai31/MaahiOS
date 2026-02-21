/**
 * MaahiOS RTC (Real-Time Clock) Driver
 * 
 * Reads date/time from CMOS battery-backed RTC.
 * Registers with Device Manager for unified access.
 */

#ifndef RTC_H
#define RTC_H

#include <stdint.h>

/* ============================================
 * Date/Time Structure
 * ============================================ */
typedef struct {
    uint8_t  second;     /* 0-59 */
    uint8_t  minute;     /* 0-59 */
    uint8_t  hour;       /* 0-23 */
    uint8_t  day;        /* 1-31 */
    uint8_t  month;      /* 1-12 */
    uint16_t year;       /* Full year (e.g., 2026) */
    uint8_t  weekday;    /* 1-7 (Sunday = 1) */
} rtc_datetime_t;

/* ============================================
 * IOCTL Commands
 * ============================================ */
#define RTC_IOCTL_GET_TIME      1   /* Get full datetime structure */
#define RTC_IOCTL_GET_UNIX      2   /* Get Unix timestamp (seconds since 1970) */

/* ============================================
 * Driver API
 * ============================================ */

/**
 * Initialize RTC driver and register with Device Manager.
 * @return 0 on success, negative on error
 */
int rtc_init(void);

/**
 * Read current date/time from RTC hardware.
 * @param dt Output datetime structure
 * @return 0 on success, negative on error
 */
int rtc_read_datetime(rtc_datetime_t *dt);

/**
 * Get current time as Unix timestamp (seconds since Jan 1, 1970).
 * @return Unix timestamp
 */
uint32_t rtc_get_unix_time(void);

#endif /* RTC_H */
