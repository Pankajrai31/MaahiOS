/**
 * MaahiOS Shared Time Library - shared_time.c
 * 
 * Description:
 *   Implementation of shared time access.
 *   Reads directly from SHM without syscalls.
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#include "shared_time.h"

/*=============================================================================
 * SYSCALL INTERFACE
 *===========================================================================*/

#define SYS_SHM_ATTACH  49

static inline int syscall1(int num, int a1) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(a1)
        : "memory"
    );
    return ret;
}

static inline void* shm_attach(int shm_id) {
    return (void*)syscall1(SYS_SHM_ATTACH, shm_id);
}

/*=============================================================================
 * LIBRARY STATE
 *===========================================================================*/

static shared_time_t *g_shared_time = (void*)0;

/*=============================================================================
 * LIBRARY API IMPLEMENTATION
 *===========================================================================*/

int shared_time_init(int shm_id) {
    if (shm_id < 0) return -1;
    
    g_shared_time = (shared_time_t *)shm_attach(shm_id);
    if (!g_shared_time) {
        return -1;
    }
    
    return 0;
}

uint32_t shared_time_get_unix(void) {
    if (!g_shared_time) return 0;
    
    /* Lock-free read with sequence check */
    uint32_t seq1, seq2, unix_time;
    do {
        seq1 = g_shared_time->update_seq;
        __asm__ volatile("" ::: "memory");
        
        unix_time = g_shared_time->unix_time;
        
        __asm__ volatile("" ::: "memory");
        seq2 = g_shared_time->update_seq;
    } while (seq1 != seq2 || (seq1 & 1));
    
    return unix_time;
}

uint64_t shared_time_get_ticks(void) {
    if (!g_shared_time) return 0;
    
    /* Lock-free read with sequence check */
    uint32_t seq1, seq2;
    uint64_t ticks;
    do {
        seq1 = g_shared_time->update_seq;
        __asm__ volatile("" ::: "memory");
        
        ticks = g_shared_time->ticks;
        
        __asm__ volatile("" ::: "memory");
        seq2 = g_shared_time->update_seq;
    } while (seq1 != seq2 || (seq1 & 1));
    
    return ticks;
}

int shared_time_get_datetime(uint8_t *hour, uint8_t *minute, uint8_t *second,
                              uint8_t *month, uint8_t *day) {
    if (!g_shared_time) return -1;
    if (!hour || !minute || !second || !month || !day) return -1;
    
    /* Lock-free read with sequence check */
    uint32_t seq1, seq2;
    do {
        seq1 = g_shared_time->update_seq;
        __asm__ volatile("" ::: "memory");
        
        *hour   = g_shared_time->hour;
        *minute = g_shared_time->minute;
        *second = g_shared_time->second;
        *month  = g_shared_time->month;
        *day    = g_shared_time->day;
        
        __asm__ volatile("" ::: "memory");
        seq2 = g_shared_time->update_seq;
    } while (seq1 != seq2 || (seq1 & 1));
    
    return 0;
}

shared_time_t* shared_time_get_ptr(void) {
    return g_shared_time;
}
