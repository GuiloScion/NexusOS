/* ata.c -- PIO-mode IDE/ATA driver, primary channel slave drive.
 *
 * Register map (primary channel, base = 0x1F0):
 *   0x1F0  data         (16-bit reads/writes for sector transfer)
 *   0x1F1  error / features
 *   0x1F2  sector count
 *   0x1F3  LBA 0..7
 *   0x1F4  LBA 8..15
 *   0x1F5  LBA 16..23
 *   0x1F6  drive/head:  [0:3]=LBA 24..27, [4]=DRV (0=master, 1=slave),
 *                       [5]=1, [6]=LBA mode, [7]=1
 *   0x1F7  command / status
 *   0x3F6  device control (alt status when read)
 *
 * Status bits (0x1F7):
 *   BSY  0x80   busy
 *   DRDY 0x40   drive ready
 *   DRQ  0x08   data request (sector buffer ready)
 *   ERR  0x01
 *
 * Read flow:
 *   1. Wait BSY=0.
 *   2. Select slave drive + LBA mode.
 *   3. Write LBA + count.
 *   4. Issue READ SECTORS (0x20).
 *   5. Block on completion semaphore (posted by IRQ14 handler).
 *   6. Drain 256 16-bit words from the data register.
 *
 * The IRQ14 handler reads the status register to ack the IRQ, then posts
 * the semaphore. Without that read, the controller would hold IRQ14 high
 * and the PIC would never re-fire it.
 */

#include "ata.h"
#include "io.h"
#include "idt.h"
#include "pic.h"
#include "sync.h"
#include "console.h"

#define ATA_PRIMARY_IO   0x1F0
#define ATA_PRIMARY_CTRL 0x3F6

#define REG_DATA      (ATA_PRIMARY_IO + 0)
#define REG_ERROR     (ATA_PRIMARY_IO + 1)
#define REG_SECCOUNT  (ATA_PRIMARY_IO + 2)
#define REG_LBA0      (ATA_PRIMARY_IO + 3)
#define REG_LBA1      (ATA_PRIMARY_IO + 4)
#define REG_LBA2      (ATA_PRIMARY_IO + 5)
#define REG_DRIVE     (ATA_PRIMARY_IO + 6)
#define REG_STATUS    (ATA_PRIMARY_IO + 7)
#define REG_COMMAND   (ATA_PRIMARY_IO + 7)

#define ST_BSY   0x80
#define ST_DRDY  0x40
#define ST_DRQ   0x08
#define ST_ERR   0x01

#define CMD_READ_SECTORS 0x20
#define CMD_IDENTIFY     0xEC

#define DRIVE_SLAVE_LBA  0xF0    /* 1111 0000: LBA mode, slave, bits 7+5 set */

static semaphore_t io_done;
static mutex_t     io_lock;       /* serializes commands to the controller   */
static bool        initialized = false;

/* Spin until BSY clears or we've polled enough to declare the drive dead.
 * On real hardware you'd add a wall-clock timeout; QEMU responds in
 * microseconds, so a bounded poll suffices here. */
static bool wait_not_busy(void) {
    for (int i = 0; i < 1000000; i++) {
        uint8_t s = inb(REG_STATUS);
        if (!(s & ST_BSY)) return true;
    }
    return false;
}

/* 400 ns delay: the standard "read alt status four times" trick. */
static void ata_400ns_delay(void) {
    (void)inb(ATA_PRIMARY_CTRL);
    (void)inb(ATA_PRIMARY_CTRL);
    (void)inb(ATA_PRIMARY_CTRL);
    (void)inb(ATA_PRIMARY_CTRL);
}

static void ata_irq(interrupt_frame_t *frame) {
    (void)frame;
    pic_send_eoi(14);
    /* Reading status clears the controller's INTRQ line. */
    (void)inb(REG_STATUS);
    sem_post(&io_done);
}

bool ata_init(void) {
    sem_init(&io_done, 0);
    mutex_init(&io_lock);

    /* Disable controller interrupts during init. The IDENTIFY command
     * completes asynchronously and would otherwise queue an IRQ in the
     * PIC (masked at this point but latched in IRR) that would fire
     * spuriously the moment we unmask IRQ14, pre-posting our semaphore
     * and making the first real read return before its data was ready.
     * nIEN (bit 1 of the device control reg) blocks the device from
     * asserting INTRQ at all. */
    outb(ATA_PRIMARY_CTRL, 0x02);

    /* Select slave drive and let it settle. */
    outb(REG_DRIVE, DRIVE_SLAVE_LBA);
    ata_400ns_delay();

    /* Run IDENTIFY against the slave to confirm something's there. */
    outb(REG_SECCOUNT, 0);
    outb(REG_LBA0,     0);
    outb(REG_LBA1,     0);
    outb(REG_LBA2,     0);
    outb(REG_COMMAND,  CMD_IDENTIFY);

    uint8_t s = inb(REG_STATUS);
    if (s == 0) {
        console_puts("[ata] no slave device on primary channel\n");
        return false;
    }

    /* Wait for either DRQ (data ready) or ERR. */
    while (inb(REG_STATUS) & ST_BSY) { }
    s = inb(REG_STATUS);
    if (s & ST_ERR) {
        console_puts("[ata] identify error\n");
        return false;
    }
    /* Drain the 256-word identify block; we don't actually use it, but
     * leaving data in the controller would jam the next command. */
    for (int i = 0; i < 256; i++) (void)inw(REG_DATA);

    /* Re-enable interrupts on the controller now that init's done. */
    outb(ATA_PRIMARY_CTRL, 0x00);

    irq_register(14, ata_irq);
    pic_unmask(14);

    initialized = true;
    console_puts("[ata] primary slave ready\n");
    return true;
}

bool ata_read_sector(uint32_t lba, void *buf) {
    if (!initialized) return false;
    if (lba >> 28)     return false;   /* 28-bit LBA only */

    mutex_lock(&io_lock);

    if (!wait_not_busy()) { mutex_unlock(&io_lock); return false; }

    outb(REG_DRIVE,    (uint8_t)(DRIVE_SLAVE_LBA | ((lba >> 24) & 0x0F)));
    ata_400ns_delay();
    outb(REG_SECCOUNT, 1);
    outb(REG_LBA0,     (uint8_t)( lba        & 0xFF));
    outb(REG_LBA1,     (uint8_t)((lba >>  8) & 0xFF));
    outb(REG_LBA2,     (uint8_t)((lba >> 16) & 0xFF));
    outb(REG_COMMAND,  CMD_READ_SECTORS);

    /* Block until IRQ14 says the sector buffer is ready. */
    sem_wait(&io_done);

    uint8_t status = inb(REG_STATUS);
    if (status & ST_ERR) {
        mutex_unlock(&io_lock);
        return false;
    }
    if (!(status & ST_DRQ)) {
        mutex_unlock(&io_lock);
        return false;
    }

    uint16_t *p = (uint16_t *)buf;
    for (int i = 0; i < 256; i++) p[i] = inw(REG_DATA);

    mutex_unlock(&io_lock);
    return true;
}
