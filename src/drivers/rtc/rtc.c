/**
 * MaahiOS RTC (Real-Time Clock) Driver
 * 
 * Reads date/time from CMOS battery-backed RTC (MC146818 compatible).
 * QEMU syncs this with host machine time.
 */

#include "rtc.h"
#include "../../managers/device/device_manager.h"
#include "../../managers/klog/klog.h"

/* ===========================================================================
 * CMOS RTC Ports and Registers
 * =========================================================================== */

#define CMOS_ADDR_PORT  0x70
#define CMOS_DATA_PORT  0x71

/* CMOS Register addresses */
#define RTC_REG_SECONDS     0x00
#define RTC_REG_MINUTES     0x02
#define RTC_REG_HOURS       0x04
#define RTC_REG_WEEKDAY     0x06
#define RTC_REG_DAY         0x07
#define RTC_REG_MONTH       0x08
#define RTC_REG_YEAR        0x09
#define RTC_REG_CENTURY     0x32  /* May not be available on all systems */
#define RTC_REG_STATUS_A    0x0A
#define RTC_REG_STATUS_B    0x0B

/* Status A bit 7: Update In Progress */
#define RTC_UIP_BIT         0x80

/* Status B bits */
#define RTC_24HR_BIT        0x02
#define RTC_BINARY_BIT      0x04

/* ===========================================================================
 * Port I/O
 * =========================================================================== */

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* ===========================================================================
 * CMOS Read Helpers
 * =========================================================================== */

static uint8_t cmos_read(uint8_t reg) {
    outb(CMOS_ADDR_PORT, reg);
    return inb(CMOS_DATA_PORT);
}

/* Wait for RTC update to complete (avoid reading during update) */
static void rtc_wait_for_update(void) {
    /* Wait while update in progress */
    while (cmos_read(RTC_REG_STATUS_A) & RTC_UIP_BIT) {
        __asm__ volatile("pause");
    }
}

/* Convert BCD to binary */
static uint8_t bcd_to_binary(uint8_t bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

/* ===========================================================================
 * RTC Reading
 * =========================================================================== */

int rtc_read_datetime(rtc_datetime_t *dt) {
    if (!dt) return -1;
    
    /* Wait for any update to complete */
    rtc_wait_for_update();
    
    /* Read all registers */
    uint8_t second  = cmos_read(RTC_REG_SECONDS);
    uint8_t minute  = cmos_read(RTC_REG_MINUTES);
    uint8_t hour    = cmos_read(RTC_REG_HOURS);
    uint8_t day     = cmos_read(RTC_REG_DAY);
    uint8_t month   = cmos_read(RTC_REG_MONTH);
    uint8_t year    = cmos_read(RTC_REG_YEAR);
    uint8_t weekday = cmos_read(RTC_REG_WEEKDAY);
    uint8_t century = cmos_read(RTC_REG_CENTURY);
    
    /* Read status B to check format */
    uint8_t status_b = cmos_read(RTC_REG_STATUS_B);
    
    /* Convert from BCD if needed */
    if (!(status_b & RTC_BINARY_BIT)) {
        second  = bcd_to_binary(second);
        minute  = bcd_to_binary(minute);
        hour    = bcd_to_binary(hour & 0x7F);  /* Mask PM bit if 12hr */
        day     = bcd_to_binary(day);
        month   = bcd_to_binary(month);
        year    = bcd_to_binary(year);
        weekday = bcd_to_binary(weekday);
        century = bcd_to_binary(century);
    }
    
    /* Handle 12-hour format */
    if (!(status_b & RTC_24HR_BIT)) {
        uint8_t pm = cmos_read(RTC_REG_HOURS) & 0x80;
        if (pm && hour != 12) hour += 12;
        if (!pm && hour == 12) hour = 0;
    }
    
    /* Fill structure */
    dt->second  = second;
    dt->minute  = minute;
    dt->hour    = hour;
    dt->day     = day;
    dt->month   = month;
    dt->weekday = weekday;
    
    /* Calculate full year */
    if (century == 0) century = 20;  /* Default to 2000s */
    dt->year = century * 100 + year;
    
    return 0;
}

/* Days in each month (non-leap year) */
static const uint16_t days_before_month[] = {
    0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
};

static int is_leap_year(uint16_t year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

uint32_t rtc_get_unix_time(void) {
    rtc_datetime_t dt;
    rtc_read_datetime(&dt);
    
    /* Calculate days since 1970 */
    uint32_t days = 0;
    
    /* Years since 1970 */
    for (uint16_t y = 1970; y < dt.year; y++) {
        days += is_leap_year(y) ? 366 : 365;
    }
    
    /* Months in current year */
    days += days_before_month[dt.month - 1];
    
    /* Add leap day if applicable */
    if (dt.month > 2 && is_leap_year(dt.year)) {
        days++;
    }
    
    /* Days in current month */
    days += dt.day - 1;
    
    /* Convert to seconds and add time */
    uint32_t seconds = days * 86400UL;
    seconds += dt.hour * 3600UL;
    seconds += dt.minute * 60UL;
    seconds += dt.second;
    
    return seconds;
}

/* ===========================================================================
 * Device Manager Interface
 * =========================================================================== */

static int rtc_dev_open(int flags) {
    (void)flags;
    return 0;  /* Always succeeds */
}

static int rtc_dev_close(int handle) {
    (void)handle;
    return 0;
}

static int rtc_dev_read(int handle, void *buffer, size_t size) {
    (void)handle;
    
    if (!buffer || size < sizeof(rtc_datetime_t)) {
        return DEV_ERR_INVALID;
    }
    
    int ret = rtc_read_datetime((rtc_datetime_t *)buffer);
    if (ret < 0) return DEV_ERR_IO;
    
    return sizeof(rtc_datetime_t);
}

static int rtc_dev_ioctl(int handle, int cmd, void *arg) {
    (void)handle;
    
    switch (cmd) {
        case RTC_IOCTL_GET_TIME:
            if (!arg) return DEV_ERR_INVALID;
            return rtc_read_datetime((rtc_datetime_t *)arg);
        
        case RTC_IOCTL_GET_UNIX:
            if (!arg) return DEV_ERR_INVALID;
            *(uint32_t *)arg = rtc_get_unix_time();
            return 0;
        
        default:
            return DEV_ERR_NOT_SUPPORTED;
    }
}

/* Device operations table */
static device_ops_t rtc_ops = {
    .open  = rtc_dev_open,
    .close = rtc_dev_close,
    .read  = rtc_dev_read,
    .write = (void *)0,
    .ioctl = rtc_dev_ioctl,
    .poll  = (void *)0
};

/* ===========================================================================
 * Initialization
 * =========================================================================== */

int rtc_init(void) {
    /* Read initial time to verify RTC is working */
    rtc_datetime_t dt;
    int ret = rtc_read_datetime(&dt);
    if (ret < 0) {
        KLOG_ERROR("RTC", "Failed to read RTC");
        return -1;
    }
    
    /* Register with Device Manager */
    ret = register_device(DEV_RTC, "rtc", &rtc_ops);
    if (ret < 0) {
        KLOG_ERROR("RTC", "Failed to register device");
        return -1;
    }
    
    KLOG_INFO("RTC", "Driver initialized");
    return 0;
}
