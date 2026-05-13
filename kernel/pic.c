/* pic.c -- 8259A master/slave PIC.
 *
 * The pair lives at I/O ports 0x20/0x21 (master) and 0xA0/0xA1 (slave).
 * The slave's INT line is wired to the master's IRQ2.
 *
 * After reset the BIOS leaves them mapped to vectors 0x08-0x0F, which
 * collide with CPU exceptions. We remap to 0x20-0x2F via the standard
 * four-byte ICW init sequence.
 */

#include "pic.h"
#include "io.h"

#define PIC_M_CMD       0x20
#define PIC_M_DATA      0x21
#define PIC_S_CMD       0xA0
#define PIC_S_DATA      0xA1

#define ICW1_INIT       0x11    /* init + ICW4 needed */
#define ICW4_8086       0x01    /* 8086 (not 8080) mode */
#define EOI             0x20

void pic_init(void) {
    /* Save current masks. */
    uint8_t mask_m = inb(PIC_M_DATA);
    uint8_t mask_s = inb(PIC_S_DATA);

    /* ICW1: start init in cascade mode, expect ICW4. */
    outb(PIC_M_CMD, ICW1_INIT); io_wait();
    outb(PIC_S_CMD, ICW1_INIT); io_wait();

    /* ICW2: vector offset. */
    outb(PIC_M_DATA, IRQ_BASE);        io_wait();   /* 0x20..0x27 */
    outb(PIC_S_DATA, IRQ_BASE + 8);    io_wait();   /* 0x28..0x2F */

    /* ICW3: tell master that slave is on IRQ2 (bit 2 = 0b0000_0100);
     *       tell slave its cascade identity is 2. */
    outb(PIC_M_DATA, 0x04); io_wait();
    outb(PIC_S_DATA, 0x02); io_wait();

    /* ICW4: 8086 mode. */
    outb(PIC_M_DATA, ICW4_8086); io_wait();
    outb(PIC_S_DATA, ICW4_8086); io_wait();

    /* Restore masks (caller will unmask what it wants). */
    outb(PIC_M_DATA, mask_m);
    outb(PIC_S_DATA, mask_s);

    /* Default: mask everything. */
    outb(PIC_M_DATA, 0xFF);
    outb(PIC_S_DATA, 0xFF);
}

void pic_mask(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC_M_DATA : PIC_S_DATA;
    uint8_t  bit  = (uint8_t)(1u << (irq & 7));
    outb(port, (uint8_t)(inb(port) | bit));
}

void pic_unmask(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC_M_DATA : PIC_S_DATA;
    uint8_t  bit  = (uint8_t)(1u << (irq & 7));
    outb(port, (uint8_t)(inb(port) & ~bit));
    /* If we're unmasking a slave IRQ, also unmask cascade line. */
    if (irq >= 8) {
        outb(PIC_M_DATA, (uint8_t)(inb(PIC_M_DATA) & ~(1u << 2)));
    }
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) outb(PIC_S_CMD, EOI);
    outb(PIC_M_CMD, EOI);
}

void pic_disable(void) {
    outb(PIC_M_DATA, 0xFF);
    outb(PIC_S_DATA, 0xFF);
}
