# Glossary

The jargon of OS development, in plain language. Terms are grouped by topic.

## Booting & CPU modes

- **BIOS** — the old PC firmware that runs at power-on, in 16-bit real mode, and
  provides services (disk, video, memory map) via software interrupts.
- **UEFI** — the modern firmware replacing BIOS. Boots differently; a BIOS MBR OS
  needs **CSM** to run on it.
- **CSM (Compatibility Support Module)** — a UEFI feature that re-enables
  legacy/BIOS booting. Must be on to boot a BIOS OS.
- **MBR (Master Boot Record)** — the first 512-byte sector of a boot device. The
  firmware loads it to `0x7C00` and runs it. Ends with the signature `0x55AA`.
- **Bootloader** — the first code *you* write; lives in the MBR, sets things up,
  and loads your kernel.
- **Real mode** — the CPU's 16-bit startup mode. 1 MB of memory, BIOS available,
  no protection.
- **Protected mode** — 32-bit mode with memory protection and segmentation.
- **Long mode** — 64-bit mode. Requires paging to be enabled.
- **Ring 0 / Ring 3** — CPU privilege levels. Ring 0 = kernel (full power),
  ring 3 = user programs (restricted).
- **A20 line** — a historical address-line gate that must be enabled to access
  memory above 1 MB.
- **GDT (Global Descriptor Table)** — defines memory segments and their
  privilege levels; required to enter protected/long mode.

## Memory

- **Physical vs virtual memory** — physical is the real RAM addresses; virtual is
  what code sees, translated by paging.
- **Paging** — the hardware mechanism that maps virtual pages to physical frames
  via page tables.
- **Frame / page** — a fixed-size chunk of physical (frame) or virtual (page)
  memory, 4 KiB on x86.
- **Page table / PML4** — the tree the CPU walks to translate addresses; on
  x86-64 it has 4 levels, the top being the PML4.
- **TLB** — a CPU cache of recent address translations; flushed with `invlpg`.
- **E820** — the BIOS service (`int 15h, AX=E820`) that reports the memory map:
  which regions are usable RAM vs reserved.
- **Heap** — the region your `kmalloc`/`malloc` hands out from.
- **MMIO (memory-mapped I/O)** — hardware (like the framebuffer) exposed as
  memory addresses you read/write.

## Interrupts & timing

- **IDT (Interrupt Descriptor Table)** — maps interrupt/exception vectors to
  handler functions.
- **ISR (Interrupt Service Routine)** — the handler that runs on an interrupt.
- **IRQ** — a hardware interrupt request line (keyboard = IRQ1, timer = IRQ0,
  mouse = IRQ12, ATA = IRQ14).
- **PIC (8259A)** — the legacy interrupt controller that routes IRQs to CPU
  vectors. Remapped away from the exception vectors during init.
- **APIC** — the modern interrupt controller that replaces the PIC; needed for
  multi-core.
- **PIT (8253/8254)** — the legacy programmable interval timer; drives a periodic
  tick (and scheduler preemption).
- **EOI (End Of Interrupt)** — the signal you send the PIC so it'll deliver the
  next interrupt.

## Multitasking

- **Context switch** — saving one task's CPU registers and restoring another's.
- **Preemption** — the scheduler forcibly switching tasks (e.g. on a timer tick),
  vs cooperative yielding.
- **Mutex** — a lock; only one task holds it at a time.
- **Semaphore** — a counter for signaling/limiting; the basis of
  producer/consumer.
- **Condition variable** — lets tasks wait for a condition and be woken when it
  changes.

## Devices & storage

- **PS/2** — the legacy keyboard/mouse interface (8042 controller, ports
  `0x60`/`0x64`).
- **PIO (Programmed I/O)** — the CPU moves data to/from a device through I/O
  ports, word by word (vs DMA).
- **ATA / IDE** — the legacy disk interface; primary channel at ports
  `0x1F0–0x1F7`.
- **CHS vs LBA** — two ways to address disk sectors: Cylinder/Head/Sector (old,
  fragile) vs Logical Block Address (a flat sector number, robust).
- **Sector** — the smallest disk unit, 512 bytes.
- **Cluster** — a filesystem's allocation unit, one or more sectors.
- **FAT (File Allocation Table)** — a simple filesystem; the FAT is a linked list
  of clusters. **FAT12** is the floppy variant.
- **BPB (BIOS Parameter Block)** — the header in a FAT volume's first sector
  describing its layout.

## Graphics

- **VBE (VESA BIOS Extensions)** — the BIOS service to set graphics modes.
- **LFB (Linear Framebuffer)** — a graphics mode where the screen is one flat
  array in memory.
- **Framebuffer** — the pixel array representing the screen.
- **Pitch / stride** — bytes per scanline (row); often larger than
  `width × bytes-per-pixel` due to padding.
- **bpp (bits per pixel)** — color depth (24 or 32 here).
- **Blit** — a fast block copy of pixels (e.g. back buffer → screen).
- **Compositor** — composes multiple layers (windows, cursor) into the final
  image, typically via a back buffer.
- **Double buffering** — drawing to an off-screen buffer, then copying it to the
  screen in one go, to avoid flicker/tearing.

## Userspace & beyond

- **System call (syscall)** — a controlled entry from a ring-3 program into the
  ring-0 kernel.
- **TSS (Task State Segment)** — holds the kernel stack used when an interrupt
  occurs in user mode.
- **ELF** — the standard executable file format on x86-64; an ELF loader maps a
  program into memory and runs it.
- **VFS (Virtual File System)** — an abstraction layer so the kernel uses
  `open`/`read`/`write` regardless of the underlying filesystem.
- **SMP (Symmetric Multiprocessing)** — running on multiple CPU cores.
- **ACPI** — firmware tables describing the hardware (CPUs, interrupts, power);
  parsed to find the other cores, among other things.

## Toolchain

- **Freestanding** — code that doesn't rely on a hosted C library or OS (your
  kernel). Compiled with `-ffreestanding`.
- **Cross-compiler** — a compiler that targets a different platform than it runs
  on. (NexusOS avoids needing one by targeting ELF64 with plain host gcc flags.)
- **QEMU** — the emulator you develop against.

[← Back to Home](README.md)
