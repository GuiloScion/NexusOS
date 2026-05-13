/* idt.c
 *
 * Long-mode IDT setup and the C-level interrupt dispatcher.
 *
 * Layout of an x86-64 IDT gate descriptor (16 bytes):
 *
 *    0  offset_low   16 bits      bits  0..15 of handler address
 *    2  selector     16 bits      target code segment (kernel CS = 0x08)
 *    4  ist          3 bits       IST index (0 = use legacy stack)
 *       reserved     5 bits
 *    5  type_attr    8 bits       type=0xE (interrupt gate), P=1, DPL=0
 *    6  offset_mid   16 bits      bits 16..31 of handler address
 *    8  offset_high  32 bits      bits 32..63 of handler address
 *   12  reserved     32 bits
 */

#include "idt.h"
#include "console.h"
#include "io.h"
#include "string.h"
#include "pic.h"

typedef struct PACKED {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} idt_gate_t;

typedef struct PACKED {
    uint16_t limit;
    uint64_t base;
} idt_pointer_t;

#define IDT_SIZE        256
#define KERNEL_CS       0x18    /* 64-bit code segment (matches jmp 0x18:long_mode in kernel_entry.asm) */
#define GATE_INTERRUPT  0x8E    /* present, DPL=0, type=interrupt gate */

static idt_gate_t  idt[IDT_SIZE] ALIGN(16);
static irq_handler_t irq_handlers[16];

/* Provided by idt_stubs.asm: address of stub i lives in stub_table[i]. */
extern uintptr_t isr_stub_table[IDT_SIZE];

static void idt_set_gate(uint8_t vector, uintptr_t handler) {
    idt[vector].offset_low  = (uint16_t)(handler & 0xFFFF);
    idt[vector].selector    = KERNEL_CS;
    idt[vector].ist         = 0;
    idt[vector].type_attr   = GATE_INTERRUPT;
    idt[vector].offset_mid  = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[vector].offset_high = (uint32_t)((handler >> 32) & 0xFFFFFFFF);
    idt[vector].reserved    = 0;
}

static const char *exception_name(uint64_t v) {
    switch (v) {
        case 0:  return "#DE Divide Error";
        case 1:  return "#DB Debug";
        case 2:  return "NMI";
        case 3:  return "#BP Breakpoint";
        case 4:  return "#OF Overflow";
        case 5:  return "#BR Bound Range Exceeded";
        case 6:  return "#UD Invalid Opcode";
        case 7:  return "#NM Device Not Available";
        case 8:  return "#DF Double Fault";
        case 10: return "#TS Invalid TSS";
        case 11: return "#NP Segment Not Present";
        case 12: return "#SS Stack-Segment Fault";
        case 13: return "#GP General Protection";
        case 14: return "#PF Page Fault";
        case 16: return "#MF x87 Floating-Point";
        case 17: return "#AC Alignment Check";
        case 18: return "#MC Machine Check";
        case 19: return "#XM SIMD Floating-Point";
        case 20: return "#VE Virtualization";
        default: return "(reserved)";
    }
}

static NORETURN void panic_on_exception(interrupt_frame_t *f) {
    console_puts("\n\n=== CPU EXCEPTION ===\n");
    console_puts(exception_name(f->vector));
    console_puts("\n  vector = "); console_put_dec(f->vector);
    console_puts("\n  error  = "); console_put_hex(f->error_code);
    console_puts("\n  rip    = "); console_put_hex(f->rip);
    console_puts("\n  cs     = "); console_put_hex(f->cs);
    console_puts("\n  rflags = "); console_put_hex(f->rflags);
    console_puts("\n  rsp    = "); console_put_hex(f->rsp);
    if (f->vector == 14) {
        console_puts("\n  cr2    = "); console_put_hex(read_cr2());
    }
    console_puts("\n  rax    = "); console_put_hex(f->rax);
    console_puts("\n  rbx    = "); console_put_hex(f->rbx);
    console_puts("\n  rcx    = "); console_put_hex(f->rcx);
    console_puts("\n  rdx    = "); console_put_hex(f->rdx);
    console_puts("\n=== HALTED ===\n");
    for (;;) { cli(); hlt(); }
}

/* Called from the common assembly stub. */
void interrupt_dispatch(interrupt_frame_t *frame) {
    uint64_t v = frame->vector;

    if (v < 32) {
        panic_on_exception(frame);
    } else if (v < 32 + 16) {
        uint8_t irq = (uint8_t)(v - 32);
        if (irq_handlers[irq]) {
            irq_handlers[irq](frame);
        }
        pic_send_eoi(irq);
    }
    /* vectors >= 48 are not generated yet -- ignore. */
}

void irq_register(uint8_t irq, irq_handler_t handler) {
    if (irq < 16) irq_handlers[irq] = handler;
}

void idt_init(void) {
    /* Zero everything first. */
    uint8_t *p = (uint8_t *)idt;
    for (size_t i = 0; i < sizeof(idt); i++) p[i] = 0;

    for (size_t i = 0; i < IDT_SIZE; i++) {
        idt_set_gate((uint8_t)i, isr_stub_table[i]);
    }

    idt_pointer_t idtp;
    idtp.limit = (uint16_t)(sizeof(idt) - 1);
    idtp.base  = (uint64_t)(uintptr_t)idt;
    __asm__ volatile ("lidt %0" : : "m"(idtp));
}
