/* idt.h -- 64-bit Interrupt Descriptor Table. */
#ifndef NEXUS_IDT_H
#define NEXUS_IDT_H

#include "types.h"

/* The frame saved by every ISR stub before calling the C dispatcher.
 * Order MUST match the push sequence in idt_stubs.asm. */
typedef struct PACKED {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
    uint64_t vector;
    uint64_t error_code;
    /* These five are pushed by the CPU on interrupt entry: */
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} interrupt_frame_t;

typedef void (*irq_handler_t)(interrupt_frame_t *frame);

void idt_init(void);
void irq_register(uint8_t irq, irq_handler_t handler);

#endif
