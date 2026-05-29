# 13. Beyond: userspace, syscalls, and the road ahead

[← Real hardware](12-real-hardware.md) · [Home](README.md) · [Glossary](glossary.md)

You've built a real OS: it boots from nothing, manages memory, multitasks, talks
to disks, and draws a graphical desktop — on actual hardware. This final chapter
maps the territory beyond, so you know what each next step *is* and why it
matters. None of it is magic; it's all more of the same craft.

Up to now, everything — kernel and "tasks" alike — runs in one address space at
the highest privilege level (ring 0). The biggest leap left is the line between
**kernel** and **user programs**.

## User mode (ring 3)

x86 has four privilege rings; kernels use **ring 0** (full power) and **ring 3**
(restricted) for user programs. A ring-3 process can't run privileged
instructions, can't touch hardware ports, and can't see the kernel's memory.
Getting there involves:

- **Per-process address spaces.** Each process gets its own page tables (its own
  PML4). The kernel is mapped into every one (at high addresses), but processes
  can't see each other. Your [VMM](08-multitasking.md) already has the machinery;
  now you manage one address space per process and switch `CR3` on a context
  switch.
- **The TSS.** A Task State Segment holds the kernel stack pointer the CPU
  switches to when an interrupt arrives while in ring 3. Without it, an interrupt
  in user mode has nowhere safe to land.
- **Dropping to ring 3.** You "return" into user mode with an `iret` that sets
  the code segment's privilege bits — a controlled fall from grace.

## System calls

A ring-3 program can't do anything useful alone — it can't even print. It asks
the kernel via a **system call**: a controlled doorway from ring 3 to ring 0.
Two common mechanisms:

- A software interrupt (`int 0x80`) — simple, classic.
- The dedicated `syscall`/`sysret` instructions — faster, what modern x86-64
  kernels use.

The user puts a call number and arguments in registers, triggers the syscall,
and the kernel validates everything (never trust a user pointer!) and does the
work: `write`, `read`, `open`, `exit`, and so on.

## Running real programs: an ELF loader

To run a compiled program you must load it. **ELF** is the standard executable
format on x86-64. An **ELF loader** reads the program headers, maps each segment
into a fresh address space at the right virtual address with the right
permissions, sets up a user stack, and jumps to the entry point in ring 3. Now
you can compile a separate program, drop it on your FAT disk, and *run* it.

With syscalls + an ELF loader you have the foundation for a **shell that launches
programs**, a tiny libc, and everything that follows.

## The rest of the map

In rough order of how often people tackle them:

- **A VFS + writable filesystem.** Abstract `open`/`read`/`write` over multiple
  filesystems (Chapter 9 was read-only FAT12). Add cluster allocation for writes.
- **Better drivers.** AHCI/SATA and NVMe for modern disks; a real USB stack
  (then USB keyboards/mice work without BIOS legacy emulation — recall
  [Chapter 12](12-real-hardware.md)).
- **SMP (multiple CPUs).** Parse ACPI tables, start the other cores (APs), set up
  the **APIC** (the modern interrupt controller that replaces the PIC). Now your
  scheduler and your locks have to be genuinely multi-core-correct.
- **Networking.** A NIC driver, then the protocol stack: Ethernet → ARP → IP →
  UDP/TCP. A huge, rewarding world of its own.
- **Sound, power management, a real GUI toolkit, a package of user programs…**
  the line between "hobby OS" and "small real OS" keeps receding pleasantly.

## Porting to another architecture

Tempted to run it on a Raspberry Pi or other ARM board? Know this up front:
**an OS is tied to its CPU architecture.** NexusOS is x86-64 — its bootloader,
paging, port I/O, interrupt controller, and drivers are all x86-specific. ARM has
a completely different boot process, exception model (the GIC), and MMIO layout.

The good news: the **upper half** — your scheduler, memory *policy*, filesystem,
and compositor — is mostly portable C. A clean port splits the tree into
`arch/x86_64/` (the machine-specific bottom) and architecture-neutral code, then
writes a new `arch/arm64/` bottom. It's roughly "do the lower half again." Until
then, you can always run your x86 OS on an ARM machine **emulated** (QEMU), which
is how NexusOS runs on a Raspberry Pi.

## Where to learn more

See [Resources](B-resources.md) — the OSDev wiki, the Intel/AMD manuals, and a
few books — plus the [Glossary](glossary.md) for any term that's still fuzzy.

## You did it

If you've followed this far and have something that boots, you've joined a small
club of people who actually know what happens between pressing the power button
and a cursor blinking on screen. That understanding doesn't fade. Go break
things, fix them, and build the OS you wish existed.

[← Real hardware](12-real-hardware.md) · [Home](README.md) · [Glossary](glossary.md)
