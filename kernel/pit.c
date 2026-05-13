/* pit.c -- channel 0 of the 8254 PIT, programmed as a periodic tick. */

#include "pit.h"
#include "io.h"
#include "idt.h"
#include "pic.h"

#define PIT_CH0     0x40
#define PIT_CMD     0x43
#define PIT_FREQ    1193182u    /* base oscillator frequency, Hz */

static volatile uint64_t ticks;

static void pit_irq(interrupt_frame_t *frame) {
    (void)frame;
    ticks++;
}

void pit_init(uint32_t hz) {
    uint32_t divisor = PIT_FREQ / hz;
    if (divisor == 0) divisor = 1;
    if (divisor > 0xFFFF) divisor = 0xFFFF;

    /* Channel 0, lobyte/hibyte, mode 3 (square wave), binary. */
    outb(PIT_CMD, 0x36);
    outb(PIT_CH0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CH0, (uint8_t)((divisor >> 8) & 0xFF));

    irq_register(0, pit_irq);
    pic_unmask(0);
}

uint64_t pit_ticks(void) {
    return ticks;
}
