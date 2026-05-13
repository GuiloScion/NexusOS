/* pic.h -- Legacy 8259A PIC pair. */
#ifndef NEXUS_PIC_H
#define NEXUS_PIC_H

#include "types.h"

#define IRQ_BASE    0x20    /* IRQs remapped to vectors 0x20..0x2F */

void pic_init(void);
void pic_mask(uint8_t irq);
void pic_unmask(uint8_t irq);
void pic_send_eoi(uint8_t irq);
void pic_disable(void);

#endif
