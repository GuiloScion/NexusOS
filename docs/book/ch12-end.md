# Chapter 12. Running on real hardware (and how to debug)

Your OS boots beautifully in QEMU. Now put it on a real PC. This is where you learn that **the emulator was lying to you**, gently smoothing over a hundred messy realities that actual firmware does not. This is the most honest chapter in the book.

## Getting it onto a machine

NexusOS is a **BIOS / legacy** OS, so:

1. **Build a raw disk image** (`build/os.bin` = MBR + kernel).
2. **Write it raw to a USB stick**, *not* as a file. Use Rufus in "DD Image" mode, balenaEtcher's "Flash from file", or `dd if=os.bin of=/dev/sdX bs=512`. This puts your MBR in sector 0 of the stick.
3. **Configure firmware:** enable **Legacy / CSM** boot (a pure-UEFI machine cannot run a BIOS MBR), and enable **USB Legacy Support** (so a USB keyboard is emulated as PS/2 and your driver sees it). Boot from the stick.

## The five things that will break (they did for NexusOS)

Every one of these worked perfectly in QEMU and failed on a real ASUS desktop. They are worth internalising because they are representative of *why* real-hardware bring-up is hard.

**1. CHS disk reads hang.** The bootloader's first version loaded the kernel with the old cylinder/head/sector `int 13h` call. On real USB-boot BIOSes, a multi-sector CHS read that crosses a track boundary can hang forever, because CHS geometry is a fiction for modern USB devices, and the firmware's emulation of it is brittle. **Fix:** use the `int 13h` LBA extensions (`AH=42h`) with a Disk Address Packet, universally supported by anything that boots from USB. (This is the Listing 3.2 you already have.)

**2. The memory map is empty.** The BIOS `int 15h, E820` call returned a map with **no usable RAM**, only reserved MMIO regions. The machine obviously has memory; the firmware just did not report it the way we expected. **Fix:** when the map has no usable region, fall back to a conservative assumption. NexusOS assumes 128 MiB above 1 MiB and lives to boot another day; that is the `if (!have_usable)` branch in Listing 7.1.

**3. A missing disk hangs the boot.** ATA init probed the legacy IDE port; on a SATA/AHCI machine there is nothing there, the bus reads `0xFF`, and the "wait while busy" loop spun forever (`0xFF` has the busy bit set). **Fix:** treat `0xFF` as "no device" and bound every hardware wait loop with a timeout. This is the `wait_not_busy` guard in Listing 9.2.

**4. VBE hangs.** Re-enabling the graphics mode, the firmware *hung inside the VBE `int 10h` call*, and you cannot time-out a BIOS call that never returns. **Fix:** make graphics optional. NexusOS ships a `make text` build that skips VBE entirely; it runs perfectly in text mode on that board. The full GUI still works on QEMU and on hardware with sane VBE.

**5. Text-mode backspace printed garbage.** The framebuffer console handled backspace, but the *VGA text* console (only used on real text-mode hardware) did not, so backspace printed character `0x08`, a little box glyph. A one-line fix, but you only find it on metal. This is exercise 2 from Chapter 5; the patch is the one you wrote there.

Three principles: **bound every wait loop**, **validate every firmware return value**, and **make every nice-to-have degrade gracefully.** Internalise those, retrofit them across every subsystem you have written so far, and you eliminate the entire class. Defensive code is the difference between "works in QEMU" and "works."

## Debugging on real hardware

On real hardware you often have *no output* when something fails early. Build a toolkit:

- **Serial first.** Mirror all kernel output to the serial port (COM1). On a real machine, a serial cable or a PCI/USB serial adapter gives you a log even when the screen is dead. In QEMU it is just `-serial stdio`. This is your number-one tool.
- **Stage markers.** When you suspect a hang in early boot (before the console is up), print a single character after each stage (`1` after the memory map, `M`, `K`, `P`, …). The last character you see tells you exactly where it died. NexusOS used this to pinpoint a hang to "inside the VBE call."
- **GDB plus QEMU.** `make debug` launches QEMU paused (`-s -S`) and attaches GDB with symbols and a breakpoint on `kernel_main`. You can single-step the kernel, inspect registers, and watch page tables, a luxury you lose on bare metal.
- **`screendump`.** QEMU's monitor can dump the framebuffer to an image without a window, perfect for headless or automated checks (`tools/screenshot.sh`).
- **Distinct banners.** Re-flashing a USB stick can *silently fail* (the write does not take). If two builds share a banner, you cannot tell which one booted. Change the banner text per build so a stale flash is obvious. NexusOS lost an hour to a flaky stick before adding this.

## A serial-only panic dump

Your Chapter 6 exception handler already writes to `console_putc`, which writes to both serial and screen. On a machine where the screen has gone black (because VBE failed, or you switched modes and forgot to draw anything) the serial copy of the panic dump *is the only thing you have*. Treat it as a precious artefact: capture every byte and read it carefully.

When the screen and serial both die, the next move is to put a logic analyser on the serial line or attach an external display via a PCI card. But in practice, if you have done the stage-marker discipline above, you will have caught the problem well before either of those is necessary.

You have a real OS on real metal. The next chapter is the roadmap for what to build on top of it.

## Exercises

1. **Read your own panic dump.** With your IDE/ATA, IDT, and PMM in place, deliberately write `*(uint8_t*)0 = 1;` from a kernel task. The exception handler should print the page-fault dump to both screen and serial. Save the serial log to a file with `qemu-system-x86_64 -serial file:panic.log` and confirm the dump is identical.

   *Hint:* If you only see partial output on serial, check that you have called `serial_init()` early, before any subsystem that might fault. The framebuffer console may also chew up output if it has not been initialised yet; the serial path is the safe one.

2. **Boot it on the oldest spare PC you can find.** Old machines with real BIOS (and even PS/2 ports) are the friendliest targets. A 2008-era ThinkPad or similar will likely boot first try. A 2020-era pure-UEFI laptop without CSM almost certainly will not; that machine needs the next chapter's worth of new code (UEFI support, ELF loader, the rest).

   *Hint:* Even on a sympathetic machine, you may need to disable Secure Boot. Look under "Security" or "Boot" in the firmware setup. Take a phone photo of every setting before you change it.

3. **Set up serial capture from a real PC.** Connect a serial-to-USB adapter to a host (any laptop) and run `screen /dev/ttyUSB0 115200` (Linux/macOS) or PuTTY (Windows) at the same baud rate. Boot your OS on the target. The serial console should mirror everything. This is the difference between "I have no idea what failed" and "here is the exact line, with registers."

   *Hint:* Some boards lack a real COM port; you can buy a $10 PCI-Express serial card that works under Legacy/CSM. Mini-PCs without a serial header are out of luck. For those, USB serial adapters work too, but require driver support inside your OS (which you have not built yet), so you would be debugging the debug channel. Stick to boards with native serial for your first real-hardware push.

4. **Bring up text mode on the worst board you have.** Build with `make text` (no VBE) and try the boards that hung in graphics mode. If text mode boots and gives you a shell, you have proven the rest of the kernel works on that board: the VBE hang is purely a firmware-specific graphics issue, isolable from everything else. That separation of concerns is itself a result worth having.

   *Hint:* The `TEXT_ONLY` define in the bootloader (see the `%ifdef TEXT_ONLY` in `bootloader/boot.asm`) skips the VBE calls entirely and sets the framebuffer descriptor's valid flag to `0`. The kernel sees that and stays on the VGA text console. No code changes, just a build flag.

# Chapter 13. What to build next

You have a kernel that boots, manages memory, multitasks, talks to a disk, and draws a graphical desktop on real hardware. That is a substantial machine, and it is not the end. This chapter is the roadmap for the next dozen weekends: user mode, system calls, ELF, SMP, networking, and porting. Each section is a sketch of what the thing is, why it matters, and where you'd start. The exercises pick out one concrete first move per topic. Treat them as Volume 2's table of contents, with prototype code attached.

Up to now, everything (kernel and "tasks" alike) runs in one address space at the highest privilege level (ring 0). The biggest leap left is the line between **kernel** and **user programs**.

## User mode (ring 3)

x86 has four privilege rings; kernels use **ring 0** (full power) and **ring 3** (restricted) for user programs. A ring-3 process cannot run privileged instructions, cannot touch I/O ports, and cannot see the kernel's memory. Getting there involves:

- **Per-process address spaces.** Each process gets its own page tables (its own PML4). The kernel is mapped into every one (at high addresses), but processes cannot see each other. Your VMM from Chapter 7 already has the machinery; now you manage one address space per process and switch `CR3` on a context switch.
- **The TSS.** A *Task State Segment* holds the kernel stack pointer the CPU switches to when an interrupt arrives while in ring 3. Without it, an interrupt in user mode has nowhere safe to land.
- **Dropping to ring 3.** You "return" into user mode with an `iretq` whose saved `cs` selector has its low two bits set to `3`, a controlled fall from grace. The interrupt frame format is the same one your task creation already builds; only the selectors differ.

## System calls

A ring-3 program cannot do anything useful alone; it cannot even print. It asks the kernel via a **system call**: a controlled doorway from ring 3 to ring 0. Two common mechanisms:

- A software interrupt (`int 0x80`), simple, classic, and exactly what NexusOS already uses internally for `sched_yield`.
- The dedicated `syscall` / `sysret` instructions, faster, and what modern x86-64 kernels use. They use a model-specific register (MSR) to point at the kernel entry point and switch privilege rings without an IDT lookup.

The user puts a call number and arguments in registers, triggers the syscall, and the kernel validates everything (*never trust a user pointer!*) and does the work: `write`, `read`, `open`, `exit`, and so on. The validation step is where new kernels acquire most of their security vulnerabilities; copying user data with care (`copy_from_user` style helpers that page-fault gracefully) is its own small art.

## Running real programs: an ELF loader

To run a compiled program, you must load it. **ELF** is the standard executable format on x86-64. An **ELF loader** reads the program headers, maps each segment into a fresh address space at the right virtual address with the right permissions, sets up a user stack, and jumps to the entry point in ring 3. Now you can compile a separate program with `gcc`, drop it on your FAT disk, and *run* it.

With syscalls plus an ELF loader you have the foundation for a **shell that launches programs**, a tiny libc, and everything that follows.

## The rest of the map

In rough order of how often people tackle them:

- **A VFS plus writable filesystem.** Abstract `open` / `read` / `write` over multiple filesystems (Chapter 9 was read-only FAT12). Add cluster allocation for writes.
- **Better drivers.** AHCI/SATA and NVMe for modern disks; a real USB stack (then USB keyboards and mice work without BIOS legacy emulation; recall Chapter 12).
- **SMP (multiple CPUs).** Parse ACPI tables, start the other cores ("APs"), set up the **APIC** (the modern interrupt controller that replaces the PIC). Now your scheduler and your locks have to be genuinely multi-core correct; single-CPU `cli`/`sti` does not protect you from another CPU running the same critical section.
- **Networking.** A NIC driver, then the protocol stack: Ethernet → ARP → IP → UDP/TCP. A huge, rewarding world of its own; *Beej's Guide* and the BSD TCP stack source are good companions.
- **Sound, power management, a real GUI toolkit, a package of user programs.** The line between "hobby OS" and "small real OS" keeps receding pleasantly.

## Porting to another architecture

Tempted to run it on a Raspberry Pi or other ARM board? Know this up front: **an OS is tied to its CPU architecture.** NexusOS is x86-64; its bootloader, paging, port I/O, interrupt controller, and drivers are all x86-specific. ARM has a completely different boot process, exception model (the GIC), and MMIO layout.

The good news: the **upper half** (your scheduler, your memory *policy*, your filesystem, and your compositor) is mostly portable C. A clean port splits the tree into `arch/x86_64/` (the machine-specific bottom) and architecture-neutral code, then writes a new `arch/arm64/` bottom. It is roughly "do the lower half again." Until then, you can always run your x86 OS on an ARM machine *emulated* with QEMU, which is how NexusOS runs on a Raspberry Pi.

## Picking what to build first

You do not have to do these in order. Pick the one that scratches the itch you came to this book with. The exercises below give you a concrete starting move for each of the major directions; any one of them is a weekend's worth of solid work and ends with something visibly new running on top of the kernel you already have. Pick the first one, ship it, send a PR, pick the next.

## Exercises

1. **Drop one task to ring 3, and watch it fault.** Modify `task_create` to take a `bool user` flag; if true, set `cs = 0x23` (user code selector) and `ss = 0x1B` (user data selector) in the initial frame, after extending the GDT with ring-3 segment descriptors. Run a task that simply tries to `outb(0x3F8, 'X')`. You should get a `#GP General Protection` exception: ring 3 cannot do I/O. That panic is your privilege check working.

   *Hint:* Without a TSS, the moment an interrupt fires in ring 3, the CPU has nowhere to land and you triple-fault. Set up the TSS *first*; then try the experiment.

2. **Implement a single syscall.** Add an `int 0x80` handler that, when invoked with `rax = 1`, takes a pointer in `rdi` and a length in `rsi` and writes that many bytes to the console. From a ring-3 task, call it. Now your kernel has its first syscall: `write`. Add `exit` (`rax = 2`) next, then `read` (`rax = 3`) for the keyboard ring buffer, and you have a Unix-shaped system in miniature.

   *Hint:* Validate the pointer! `copy_from_user(dst, src, n)` should walk `src` page by page and refuse if any page is not user-mapped. Otherwise a user program can pass `rdi = 0xFFFFFFFFC0000000` and trick the kernel into reading kernel memory on its behalf.

3. **Write the smallest possible userspace `hello`.** A C program that calls your `write` syscall via inline assembly, then your `exit` syscall, with no libc. Compile freestanding (yes, the same flags as the kernel), link statically, run it through your ELF loader. The output of a complete OS booting and running a separate compiled program is a milestone worth pausing on.

   *Hint:* The smallest entry is `void _start() { write(1, "hello\n", 6); exit(0); }`. Pass `-nostdlib -static -fno-pic -e _start` to the linker and you will get an ELF that only depends on your syscalls.

4. **Sketch the SMP startup sequence on paper.** You do not have to write the code; just trace what *would* happen. The bootstrap processor parses ACPI's MADT table to learn the LAPIC IDs of the other cores; sends an INIT-SIPI-SIPI sequence to each; each AP starts in real mode at a 4 KiB-aligned address you supply, runs the same mode-switch ritual from Chapter 4, and joins the scheduler ring. What in your scheduler breaks the moment two CPUs run it concurrently? (Hint: every `cli`/`sti` in `sync.c`.) Now you know what a *spinlock* is for.

   *Hint:* The answer is "every critical region that today uses `cli`/`sti` is broken." On SMP you need a spinlock: a tiny atomic-test-and-set loop that prevents *another core* from entering the region, since disabling interrupts on your own core does not stop the other core's CPU. SMP is the chapter where every primitive you have built grows a second half.

# Appendix A. Glossary

The jargon of OS development, in plain language. Each entry has a *(Ch N)* pointer to the chapter where the term is first used in earnest. Terms are grouped by topic.

## Booting and CPU modes

**BIOS** *(Ch 3).* The old PC firmware that runs at power-on, in 16-bit real mode, and provides services (disk, video, memory map) via software interrupts.

**UEFI** *(Ch 12).* The modern firmware that replaces BIOS. Boots differently; a BIOS MBR OS needs **CSM** to run on it.

**CSM (Compatibility Support Module)** *(Ch 12).* A UEFI feature that re-enables legacy/BIOS booting. Must be on to boot a BIOS OS.

**MBR (Master Boot Record)** *(Ch 3).* The first 512-byte sector of a boot device. The firmware loads it to `0x7C00` and runs it. Ends with the signature `0x55 0xAA`.

**Bootloader** *(Ch 3).* The first code *you* write; lives in the MBR, sets things up, and loads your kernel.

**Real mode** *(Ch 3).* The CPU's 16-bit startup mode. 1 MB of memory, BIOS available, no protection.

**Protected mode** *(Ch 4).* 32-bit mode with memory protection and segmentation.

**Long mode** *(Ch 4).* 64-bit mode. Requires paging to be enabled.

**Ring 0 / Ring 3** *(Ch 13).* CPU privilege levels. Ring 0 is the kernel (full power), ring 3 is user programs (restricted).

**A20 line** *(Ch 4).* A historical address-line gate that must be enabled to access memory above 1 MB.

**GDT (Global Descriptor Table)** *(Ch 4).* Defines memory segments and their privilege levels; required to enter protected and long mode.

## Memory

**Physical vs virtual memory** *(Ch 7).* Physical is the real RAM addresses; virtual is what code sees, translated by paging.

**Paging** *(Chs 4, 7).* The hardware mechanism that maps virtual pages to physical frames via page tables.

**Frame / page** *(Ch 7).* A fixed-size chunk of physical (frame) or virtual (page) memory, 4 KiB on x86.

**Page table / PML4** *(Chs 4, 7).* The tree the CPU walks to translate addresses; on x86-64 it has 4 levels, the top being the PML4.

**TLB** *(Ch 7).* A CPU cache of recent address translations; flushed with `invlpg`.

**E820** *(Chs 3, 7).* The BIOS service (`int 15h, AX=E820`) that reports the memory map: which regions are usable RAM vs reserved.

**Heap** *(Ch 7).* The region your `kmalloc` / `malloc` hands out from.

**MMIO (memory-mapped I/O)** *(Ch 10).* Hardware (like the framebuffer) exposed as memory addresses you read and write.

## Interrupts and timing

**IDT (Interrupt Descriptor Table)** *(Ch 6).* Maps interrupt and exception vectors to handler functions.

**ISR (Interrupt Service Routine)** *(Ch 6).* The handler that runs on an interrupt.

**IRQ** *(Ch 6).* A hardware interrupt request line (keyboard = IRQ1, timer = IRQ0, mouse = IRQ12, ATA = IRQ14).

**PIC (8259A)** *(Ch 6).* The legacy interrupt controller that routes IRQs to CPU vectors. Remapped away from the exception vectors during init.

**APIC** *(Ch 13).* The modern interrupt controller that replaces the PIC; needed for multi-core.

**PIT (8253 / 8254)** *(Ch 6).* The legacy programmable interval timer; drives a periodic tick (and scheduler preemption).

**EOI (End Of Interrupt)** *(Ch 6).* The signal you send the PIC so it will deliver the next interrupt.

## Multitasking

**Context switch** *(Ch 8).* Saving one task's CPU registers and restoring another's.

**Preemption** *(Ch 8).* The scheduler forcibly switching tasks (e.g. on a timer tick), versus cooperative yielding.

**Mutex** *(Ch 8).* A lock; only one task holds it at a time.

**Semaphore** *(Ch 8).* A counter for signalling and limiting; the basis of producer/consumer patterns.

**Condition variable** *(Ch 8).* Lets tasks wait for a condition and be woken when it changes.

## Devices and storage

**PS/2** *(Chs 6, 11).* The legacy keyboard / mouse interface (8042 controller, ports `0x60` / `0x64`).

**PIO (Programmed I/O)** *(Ch 9).* The CPU moves data to and from a device through I/O ports, word by word (vs DMA).

**ATA / IDE** *(Ch 9).* The legacy disk interface; primary channel at ports `0x1F0`–`0x1F7`.

**CHS vs LBA** *(Chs 3, 9).* Two ways to address disk sectors: Cylinder/Head/Sector (old, fragile) versus Logical Block Address (a flat sector number, robust).

**Sector** *(Ch 9).* The smallest disk unit, 512 bytes.

**Cluster** *(Ch 9).* A filesystem's allocation unit, one or more sectors.

**FAT (File Allocation Table)** *(Ch 9).* A simple filesystem; the FAT is a linked list of clusters. FAT12 is the floppy variant.

**BPB (BIOS Parameter Block)** *(Ch 9).* The header in a FAT volume's first sector describing its layout.

## Graphics

**VBE (VESA BIOS Extensions)** *(Ch 10).* The BIOS service to set graphics modes.

**LFB (Linear Framebuffer)** *(Ch 10).* A graphics mode where the screen is one flat array in memory.

**Framebuffer** *(Ch 10).* The pixel array representing the screen.

**Pitch / stride** *(Ch 10).* Bytes per scanline (row); often larger than `width * bytes_per_pixel` due to padding.

**bpp (bits per pixel)** *(Ch 10).* Colour depth (24 or 32 here).

**Blit** *(Ch 11).* A fast block copy of pixels (e.g. back buffer to screen).

**Compositor** *(Ch 11).* Composes multiple layers (windows, cursor) into the final image, typically via a back buffer.

**Double buffering** *(Ch 11).* Drawing to an off-screen buffer, then copying it to the screen in one go, to avoid flicker and tearing.

## Userspace and beyond

**System call (syscall)** *(Ch 13).* A controlled entry from a ring-3 program into the ring-0 kernel.

**TSS (Task State Segment)** *(Ch 13).* Holds the kernel stack used when an interrupt occurs in user mode.

**ELF** *(Ch 13).* The standard executable file format on x86-64; an ELF loader maps a program into memory and runs it.

**VFS (Virtual File System)** *(Ch 13).* An abstraction layer so the kernel uses `open` / `read` / `write` regardless of the underlying filesystem.

**SMP (Symmetric Multiprocessing)** *(Ch 13).* Running on multiple CPU cores.

**ACPI** *(Ch 13).* Firmware tables describing the hardware (CPUs, interrupts, power); parsed to find the other cores, among other things.

## Toolchain

**Freestanding** *(Ch 2).* Code that does not rely on a hosted C library or OS (your kernel). Compiled with `-ffreestanding`.

**Cross-compiler** *(Ch 2).* A compiler that targets a different platform than it runs on. (NexusOS avoids needing one by targeting ELF64 with plain host gcc flags.)

**QEMU** *(Ch 2).* The emulator you develop against.

# Appendix B. Resources and further reading

The references that make OS development tractable. You do not need all of these, but when you are stuck, one of them has the answer.

## The essential reference

**The OSDev Wiki** (`wiki.osdev.org`) is the community knowledge base for hobby OS development. Articles on the bootloader, GDT, IDT, paging, ATA, VBE, APIC, and almost everything in this book. Start here when implementing any new subsystem; cross-check its examples against the hardware manuals.

**The OSDev forums** carry searchable archives of nearly every mistake you will make, already debugged by someone else.

## The authoritative manuals

When the wiki and reality disagree, the manuals win:

**Intel 64 and IA-32 Architectures Software Developer's Manual (SDM).** The definitive word on x86 and x86-64: instructions, modes, paging, interrupts. Huge, but Volume 3 (System Programming) is what you will live in.

**AMD64 Architecture Programmer's Manual (APM).** AMD's equivalent; often clearer on long-mode specifics.

**Device datasheets:** the 8259A PIC, 8254 PIT, 8042 PS/2 controller, and the ATA/ATAPI spec. Short, and they remove all guesswork about port behaviour.

## Books and courses

**Operating Systems: Three Easy Pieces** (free online) is the best conceptual grounding in OS *ideas* (processes, memory, concurrency, filesystems). Pair its theory with this book's practice.

**xv6** (MIT) is a small, clean, well-commented teaching OS with an accompanying book. Reading xv6 alongside your own code is enormously clarifying. (It is RISC-V or older x86, but the structure transfers.)

***The Little Book About OS Development*** is a short, friendly walkthrough of early bring-up on x86. Good companion reading to Chapters 3 and 4.

## Tools

**QEMU**, your primary test machine. `-serial stdio`, `-d int,cpu_reset` (log interrupts and resets), and the monitor (`screendump`, `info registers`, `input-send-event`) are invaluable.

**GDB**: attach to QEMU (`-s -S`) to single-step the kernel. The debug build target in Chapter 2 sets this up.

**Bochs**, a slower emulator with an even more detailed internal debugger; great for diagnosing triple faults and mode-transition bugs QEMU glosses over.

**NASM, GCC, ld, objcopy, mtools**: the build toolchain from Chapter 2.

## Read the reference OS

The most useful resource for *this* book is the code it is built on. Every chapter inlines the file that implements it; reading the **NexusOS** source end to end (bootloader, kernel, window manager) is a complete, working example you can run, modify, and break.

