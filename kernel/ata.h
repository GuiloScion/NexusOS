/* ata.h -- PIO-mode driver for an IDE/ATA disk on the primary controller.
 *
 * Targets the *slave* device on the primary IDE channel (I/O base 0x1F0,
 * IRQ14). The QEMU command line wires /dev/sda-equivalent (os.bin) as the
 * master and fat.img as the slave, so the kernel never touches its own
 * boot disk, it only reads the data disk.
 *
 * 28-bit LBA addressing. 512-byte sectors. One sector per call. The
 * filesystem layer above batches into multi-sector reads if it cares.
 *
 * Reads block: the calling task sleeps on a semaphore that the IRQ14
 * handler posts when the controller signals completion. So ata_read_sector
 * is safe to call only from task context with interrupts enabled.
 */
#ifndef NEXUS_ATA_H
#define NEXUS_ATA_H

#include "types.h"

#define ATA_SECTOR_SIZE 512

/* Probe and initialize the primary slave drive. Registers the IRQ14
 * handler. Returns true on success, false if no usable drive responds. */
bool ata_init(void);

/* Read one 512-byte sector at LBA into buf. Returns true on success. */
bool ata_read_sector(uint32_t lba, void *buf);

#endif
