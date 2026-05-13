/* io.h -- x86 port I/O primitives. */
#ifndef NEXUS_IO_H
#define NEXUS_IO_H

#include "types.h"

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* A short stall used to give slow legacy hardware (8259 PIC) time to
 * latch a command before the next I/O. Writing to an unused port is
 * the traditional trick. */
static inline void io_wait(void) {
    outb(0x80, 0);
}

static inline void cli(void) { __asm__ volatile ("cli"); }
static inline void sti(void) { __asm__ volatile ("sti"); }
static inline void hlt(void) { __asm__ volatile ("hlt"); }

static inline uint64_t read_cr0(void) {
    uint64_t v; __asm__ volatile ("mov %%cr0, %0" : "=r"(v)); return v;
}
static inline void write_cr0(uint64_t v) {
    __asm__ volatile ("mov %0, %%cr0" : : "r"(v));
}
static inline uint64_t read_cr2(void) {
    uint64_t v; __asm__ volatile ("mov %%cr2, %0" : "=r"(v)); return v;
}
static inline uint64_t read_cr3(void) {
    uint64_t v; __asm__ volatile ("mov %%cr3, %0" : "=r"(v)); return v;
}
static inline void write_cr3(uint64_t v) {
    __asm__ volatile ("mov %0, %%cr3" : : "r"(v) : "memory");
}
static inline uint64_t read_cr4(void) {
    uint64_t v; __asm__ volatile ("mov %%cr4, %0" : "=r"(v)); return v;
}
static inline void write_cr4(uint64_t v) {
    __asm__ volatile ("mov %0, %%cr4" : : "r"(v));
}

static inline void invlpg(uintptr_t addr) {
    __asm__ volatile ("invlpg (%0)" : : "r"(addr) : "memory");
}

#endif
