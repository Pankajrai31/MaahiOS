/**
 * MaahiOS Time Manager
 * 
 * System-wide time tracking:
 * - Reads RTC at boot
 * - Uses PIT ticks for runtime updates
 * - Updates shared time struct for user-space access
 */

#include "time_manager.h"
#include "../klog/klog.h"
#include "../../drivers/rtc/rtc.h"

/* ===========================================================================
 * Shared Time Structure (matches system/libraries/shared/shared_time.h)
 * =========================================================================== */

typedef struct {
    volatile uint32_t lock;
    uint32_t unix_time;
    uint64_t ticks;
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;
    uint8_t  weekday;
    volatile uint32_t update_seq;
    int32_t  shm_id;
} shared_time_t;

/* ===========================================================================
 * Internal State
 * =========================================================================== */

/* Boot time from RTC (Unix timestamp) */
static uint32_t g_boot_unix_time = 0;

/* Tick when we read boot time */
static uint64_t g_boot_tick = 0;

/* Tick frequency (from PIT) */
static uint32_t g_tick_freq = 50;  /* Default 50Hz */

/* Is time system initialized? */
static int g_time_initialized = 0;

/* Shared time structure (in SHM for user-space access) */
static shared_time_t *g_shared_time = (void*)0;
static int g_shared_time_shm_id = -1;

/* ===========================================================================
 * External Dependencies
 * =========================================================================== */

/* From PIT */
extern uint64_t pit_get_ticks64(void);

/* From SHM Manager */
extern int kernel_shm_create(unsigned int size, int owner_pid);
extern unsigned int kernel_shm_attach(int shm_id, int pid, unsigned int virt_addr);

/* ===========================================================================
 * Forward Declarations
 * =========================================================================== */
static void time_update_shared(void);

/* ===========================================================================
 * Initialization
 * =========================================================================== */

int time_manager_init(void) {
    /* Read current time from RTC */
    g_boot_unix_time = rtc_get_unix_time();
    g_boot_tick = pit_get_ticks64();
    g_tick_freq = 50;  /* PIT runs at 50Hz */
    
    /* Create SHM for shared time (accessible by user-space) */
    g_shared_time_shm_id = kernel_shm_create(sizeof(shared_time_t), 0);
    if (g_shared_time_shm_id < 0) {
        KLOG_ERROR("TIME", "Failed to create shared time SHM");
        g_time_initialized = 1;  /* Still functional, just no SHM access */
        KLOG_INFO("TIME", "Manager initialized (no shared time)");
        return 0;
    }
    
    /* Attach to kernel address space */
    g_shared_time = (shared_time_t *)kernel_shm_attach(g_shared_time_shm_id, 0, 0);
    if (!g_shared_time) {
        KLOG_ERROR("TIME", "Failed to attach shared time SHM");
        g_time_initialized = 1;
        KLOG_INFO("TIME", "Manager initialized (no shared time)");
        return 0;
    }
    
    /* Initialize shared time structure */
    g_shared_time->shm_id = g_shared_time_shm_id;
    g_shared_time->lock = 0;
    g_shared_time->update_seq = 0;
    
    /* Do initial update */
    time_update_shared();
    
    g_time_initialized = 1;
    
    KLOG_INFO_HEX("TIME", "Manager initialized, shared SHM ID", g_shared_time_shm_id);
    return 0;
}

int time_is_initialized(void) {
    return g_time_initialized;
}

/* ===========================================================================
 * Time Queries
 * =========================================================================== */

uint64_t time_get_ticks(void) {
    return pit_get_ticks64();
}

uint32_t time_get_tick_frequency(void) {
    return g_tick_freq;
}

uint32_t time_get_unix(void) {
    if (!g_time_initialized) {
        return 0;
    }
    
    /* Calculate elapsed seconds since boot */
    uint64_t current_tick = pit_get_ticks64();
    uint64_t elapsed_ticks = current_tick - g_boot_tick;
    /* Cast to 32-bit for division (safe for ~2.7 years at 50Hz) */
    uint32_t ticks32 = (uint32_t)elapsed_ticks;
    uint32_t elapsed_seconds = ticks32 / g_tick_freq;
    
    return g_boot_unix_time + elapsed_seconds;
}

/* Days in each month (1-indexed, 0 = placeholder) */
static const uint8_t days_in_month[] = {
    0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

static int is_leap_year(uint16_t year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int time_get_datetime(sys_datetime_t *dt) {
    if (!dt || !g_time_initialized) {
        return -1;
    }
    
    uint32_t unix_time = time_get_unix();
    
    /* Extract time of day */
    uint32_t day_seconds = unix_time % 86400;
    dt->hour   = (uint8_t)(day_seconds / 3600);
    dt->minute = (uint8_t)((day_seconds % 3600) / 60);
    dt->second = (uint8_t)(day_seconds % 60);
    
    /* Calculate days since epoch */
    uint32_t days = unix_time / 86400;
    
    /* Thursday Jan 1, 1970 was weekday 4 (if Sunday=0) or 5 (if Sunday=1) */
    /* Using Sunday=1 convention */
    dt->weekday = (uint8_t)((days + 4) % 7 + 1);
    
    /* Calculate year */
    uint16_t year = 1970;
    while (1) {
        uint32_t days_in_year = is_leap_year(year) ? 366 : 365;
        if (days < days_in_year) break;
        days -= days_in_year;
        year++;
    }
    dt->year = year;
    
    /* Calculate month and day */
    uint8_t month = 1;
    while (month <= 12) {
        uint8_t dim = days_in_month[month];
        if (month == 2 && is_leap_year(year)) dim = 29;
        if (days < dim) break;
        days -= dim;
        month++;
    }
    dt->month = month;
    dt->day = (uint8_t)(days + 1);
    
    return 0;
}

int time_get_uptime(sys_uptime_t *up) {
    if (!up) return -1;
    
    uint64_t ticks = pit_get_ticks64();
    up->total_ticks = ticks;
    
    /* Convert to time units using 32-bit math where possible */
    /* At 50Hz: ticks * 20 = milliseconds */
    uint32_t ticks32 = (uint32_t)(ticks & 0xFFFFFFFF);  /* Use lower 32 bits */
    uint32_t total_ms = ticks32 * 20;  /* Safe for ~24 hours */
    
    up->millis  = (uint16_t)(total_ms % 1000);
    uint32_t total_secs = total_ms / 1000;
    up->seconds = (uint8_t)(total_secs % 60);
    uint32_t total_mins = total_secs / 60;
    up->minutes = (uint8_t)(total_mins % 60);
    uint32_t total_hours = total_mins / 60;
    up->hours   = (uint8_t)(total_hours % 24);
    up->days    = total_hours / 24;
    
    return 0;
}

/* ===========================================================================
 * Shared Time (for User-Space Access)
 * =========================================================================== */

/**
 * Update shared time structure
 * Called from timer tick to keep user-space time up to date
 */
static void time_update_shared(void) {
    if (!g_shared_time || !g_time_initialized) {
        return;
    }
    
    /* Update sequence for lock-free reads */
    g_shared_time->update_seq++;
    
    /* Memory barrier - ensure seq write visible before data */
    __asm__ volatile("" ::: "memory");
    
    /* Update ticks */
    g_shared_time->ticks = pit_get_ticks64();
    
    /* Update unix time */
    g_shared_time->unix_time = time_get_unix();
    
    /* Update datetime fields */
    sys_datetime_t dt;
    if (time_get_datetime(&dt) == 0) {
        g_shared_time->year    = dt.year;
        g_shared_time->month   = dt.month;
        g_shared_time->day     = dt.day;
        g_shared_time->hour    = dt.hour;
        g_shared_time->minute  = dt.minute;
        g_shared_time->second  = dt.second;
        g_shared_time->weekday = dt.weekday;
    }
    
    /* Memory barrier - ensure data writes visible before final seq */
    __asm__ volatile("" ::: "memory");
    
    /* Increment sequence again (readers check for even & unchanged) */
    g_shared_time->update_seq++;
}

/**
 * Timer tick callback - updates shared time
 * Called from PIT IRQ handler once per second (or more frequently)
 */
void time_manager_tick(void) {
    static uint32_t tick_counter = 0;
    
    tick_counter++;
    
    /* Update shared time every 50 ticks (1 second at 50Hz) */
    if (tick_counter >= 50) {
        tick_counter = 0;
        time_update_shared();
    }
}

/**
 * Get the SHM ID for shared time
 * User-space can attach to this to read time without syscalls
 */
int time_get_shared_shm_id(void) {
    return g_shared_time_shm_id;
}
