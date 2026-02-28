/**
 * MaahiOS Port I/O Helpers
 * 
 * Canonical port I/O functions for x86.
 * Include this header instead of defining local copies.
 * 
 * All functions are static inline — zero overhead, no linking needed.
 */

#ifndef SHARED_IO_H
#define SHARED_IO_H

#include <stdint.h>

/* ============================================
 * 8-bit Port I/O
 * ============================================ */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* ============================================
 * 16-bit Port I/O
 * ============================================ */
static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* ============================================
 * 32-bit Port I/O
 * ============================================ */
static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* ============================================
 * I/O Delay
 * ============================================ */
static inline void io_wait(void) {
    outb(0x80, 0);  /* Port 0x80 for brief I/O delay */
}

#endif /* SHARED_IO_H */
