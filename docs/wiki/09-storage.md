# 9. Storage: talking to a disk and reading a filesystem

[← Multitasking](08-multitasking.md) · [Home](README.md) · [Next: Graphics →](10-framebuffer-graphics.md)

A useful OS can read files. That's two layers: a **disk driver** that moves raw
sectors to/from the hardware, and a **filesystem** that interprets those sectors
as files and directories. We'll do both at the simplest useful level: a PIO ATA
driver and read-only FAT12.

## Layer 1: the disk driver (ATA PIO)

Old-style IDE/ATA disks are controlled through I/O ports. The simplest transfer
mode is **PIO** (Programmed I/O): the CPU itself moves the data, word by word,
through a data port. Slow, but dead simple — perfect for learning.

The primary ATA channel lives at ports `0x1F0–0x1F7`. To read a sector:

1. Wait until the drive isn't busy (poll the status register).
2. Select the drive and set LBA mode.
3. Write the sector count (1) and the LBA address.
4. Issue the `READ SECTORS` command (`0x20`).
5. Wait for the drive to signal data ready, then read 256 16-bit words from the
   data port.

NexusOS's `kernel/ata.c` does this **interrupt-driven**: instead of busy-waiting
for the read to finish, it `sem_wait`s on a semaphore that the IRQ14 handler
`sem_post`s when the disk is ready — so the CPU can run other tasks meanwhile.
(This is exactly the producer/consumer use of semaphores from Chapter 8.)

### Two bugs worth knowing

These are real lessons from building NexusOS:

- **Wait for BSY to clear *after* the completion interrupt** before reading the
  data/status. The interrupt only means "I have something to say" — the drive may
  still be momentarily busy. Trusting the status too early reads garbage.
- **Handle a missing drive.** On real hardware with no legacy IDE disk, the bus
  "floats" and the status register reads `0xFF` — which has the BUSY bit set, so a
  naive `while (busy)` loop **hangs forever**. Treat `0xFF` (and `0x00`) as "no
  device" and bound your wait loops with a timeout. See [HARDWARE.md](../HARDWARE.md).

These kinds of defensive checks are the difference between "works in the
emulator" and "works on a real machine."

## Layer 2: the filesystem (FAT12)

Raw sectors aren't files. A **filesystem** is the on-disk format that organizes
them. **FAT12** (the old floppy format) is the gentlest one to implement and is
trivially creatable on your host with `mtools` — ideal for learning.

A FAT12 disk has four regions, described by the **BPB** (BIOS Parameter Block) in
sector 0:

```
[ boot sector / BPB ][ FAT(s) ][ root directory ][ data area (clusters) ]
```

To read a file (`kernel/fat.c`):

1. Parse the BPB to find where each region starts and how big it is.
2. Scan the **root directory** (an array of 32-byte entries) for the filename, in
   the old "8.3" format (`HELLO   TXT`).
3. The entry gives the file's **first cluster**. Follow the **FAT chain** —
   a linked list where `FAT[n]` tells you the cluster after `n` — reading each
   cluster's sectors until you hit the end-of-chain marker.

That's enough to back `ls` (walk the root directory) and `cat <file>` (read the
cluster chain), which is exactly what NexusOS's shell exposes.

## Building a disk image to test with

You don't need a real disk. `mtools` makes a FAT image on your host with no root:

```sh
dd if=/dev/zero of=fat.img bs=1024 count=1440   # a 1.44 MB "floppy"
mformat -i fat.img -f 1440 ::                   # format it FAT12
mcopy -i fat.img hello.txt ::HELLO.TXT          # copy a file in
```

Then attach it to QEMU as a second drive. NexusOS's `Makefile` does all this and
mounts it on the second ATA channel.

## Toward a real filesystem layer

FAT12 read-only is a great start. The natural next steps (Chapter 13):

- **Write support** (allocate clusters, update the FAT).
- A **VFS** (Virtual File System) layer — an abstraction so the rest of the
  kernel says `open`/`read`/`write` without caring whether the backing store is
  FAT, or something else entirely.
- Better drivers (AHCI/SATA, NVMe) for modern disks — PIO ATA only reaches legacy
  controllers.

For now you can store and read files — and that's a satisfying capability to
have built from port pokes and sector math.

[← Multitasking](08-multitasking.md) · [Home](README.md) · [Next: Graphics →](10-framebuffer-graphics.md)
