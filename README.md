# NexusOS

A small x86-64 operating system, written from scratch. No GRUB, no Limine, no
tutorial framework — the bootloader, kernel, build system, and every line of
code are original.

## Status

Boots on QEMU. After hardware init the kernel switches to a VESA linear
framebuffer, brings up an on-screen text console, and drops you into an
interactive shell (mirrored over serial for debugging).

### What works

- **Two-stage boot.** Custom 512-byte MBR bootloader transitions real → protected →
  long mode, collects the BIOS E820 memory map, and loads the kernel.
- **Long-mode kernel** linked at `0x10000`, BSS zeroed at entry.
- **Output** on a VESA linear framebuffer (graphical text console), with the
  legacy VGA text buffer (`0xB8000`) and serial (COM1, 115200 8N1) mirrored
  alongside for early boot and debugging.
- **Interrupts.** Full 256-entry IDT, all CPU exception vectors handled,
  panic dump prints RIP, CR2, and every general-purpose register on fault.
- **PIC.** 8259A pair remapped to `0x20–0x2F`, off the way of CPU exceptions.
- **PIT.** Channel-0 timer at 100 Hz, IRQ0 maintains a tick counter.
- **Keyboard.** PS/2 scancode set 1, shift/caps/extended-key state,
  256-byte ring buffer, blocking and nonblocking reads.
- **Physical memory.** E820 map parsed in the kernel; bitmap frame allocator
  reserves the low 1 MiB, kernel image, and bitmap itself.
- **Virtual memory.** Full 4-level paging. RAM identity-mapped with 2 MiB pages
  up to detected end (capped at 4 GiB), then a kernel-owned PML4 is installed.
  4 KiB `map`/`unmap`/`translate` API.
- **Kernel heap.** First-fit doubly-linked free list, 16-byte aligned, lives at
  virtual `0x200000000` (8 GiB, above the identity map). Demand-grows via
  `pmm_alloc_frame` + `vmm_map`, coalesces on free.
- **Preemptive scheduler.** Round-robin tasks with context switch on the PIT
  tick, `sched_sleep_ms`, and an idle task.
- **Synchronization.** Mutex, counting semaphore, and condition variable, all
  built on the scheduler's BLOCKED state with FIFO-on-release hand-off.
- **Storage.** Interrupt-driven PIO ATA driver (primary slave) and a
  read-only FAT12 filesystem (`ls`, `cat`).
- **Graphics.** Bootloader sets a VESA linear-framebuffer mode (1024x768x32,
  falling back to text mode if unavailable); the kernel maps the framebuffer
  and exposes pixel/rect drawing primitives (`kernel/fb.c`).
- **Text console.** 8x8 bitmap font rendered into the framebuffer, with
  scrolling and backspace, wired into `console_*` so the shell is visible
  on screen (`kernel/fbcon.c`).
- **Interactive shell.** `help`, `ticks`, `mem`, `tasks`, `prod`, `ls`,
  `cat <file>`, `halt`.

### What's next

In rough order, toward a usable GUI:

1. PS/2 mouse driver and a hardware/software cursor.
2. Compositor / window manager — windows, z-order, dragging, redraw.
3. Widgets and a few demo apps (terminal window, etc.).
4. Syscall interface (`syscall`/`sysret`) + user mode (ring 3, ELF loader),
   so apps can run outside the kernel.
5. VFS layer over the existing FAT driver; write support.
6. SMP, ACPI parsing, APIC.

## Requirements

Toolchain (installed automatically if you use the devcontainer):

- `nasm`
- `gcc` (any reasonably modern host gcc — we don't need a cross-compiler since
  we target ELF64 and pass `-ffreestanding -m64 -mno-red-zone`)
- `ld`, `objcopy` (binutils)
- `mtools` (`mformat`, `mcopy`) — builds the FAT12 disk image without root
- `qemu-system-x86_64`
- `gdb` (optional, for `make debug`)

## Quick start

### Codespaces / devcontainer

Open the repo in GitHub Codespaces or VS Code with the Dev Containers extension.
The container provisions the full toolchain. Then:

```sh
make run
```

### Local

```sh
sudo apt install nasm gcc binutils mtools qemu-system-x86 gdb
make run
```

A QEMU window opens with the graphical console; the same log is printed on
serial:

```
==========================================
           NexusOS  -  x86_64
==========================================
[boot] console ready
[boot] idt installed
[boot] pic remapped to 0x20
[boot] interrupts enabled
[boot] kernel ends at 0x...
[pmm] total = XXX MiB, free = XXX MiB
[vmm] mapped identity ...
[heap] base=0x200000000 ...
[fb] 1024x768x24 @ 0xFD000000 pitch=3072
NexusOS ready. Type 'help' for commands.
[sched] init, idle task id=0
...
[ata] primary slave ready
[fat] mounted (...)
nexus>
```

`Ctrl-A x` exits QEMU (serial/`-display none` runs); close the window
otherwise.

### Debugging

```sh
make debug
```

Launches QEMU paused (`-s -S`) and attaches GDB with the kernel ELF loaded.
A breakpoint is set on `kernel_main`. `continue` to start.

## Graphics

The bootloader asks the BIOS (VBE) for a 1024x768 linear-framebuffer mode
while still in real mode, and leaves a small descriptor — framebuffer address,
pitch, dimensions, and bpp — at physical `0x9700` for the kernel. If VBE fails,
it sets a valid flag to 0 and the kernel stays on the text/serial console.

In the kernel, `kernel/fb.c` maps the framebuffer (it lives above identity-
mapped RAM, so it is mapped page-by-page) and provides `fb_clear`,
`fb_putpixel`, and `fb_fill_rect`. `kernel/fbcon.c` renders an 8x8 bitmap font
(`kernel/font8x8.h`) into the framebuffer and is hooked into `console_putc`, so
every `console_*` call shows up on screen as well as on serial.

### Capturing a screenshot

`tools/screenshot.sh` boots NexusOS headless, drives the QEMU monitor to grab
the framebuffer, and writes `build/screen.png`:

```sh
bash tools/screenshot.sh           # just capture the boot screen
bash tools/screenshot.sh help      # type a command first, then capture
```

## File layout

```
.
├── .devcontainer/        # Codespaces / VS Code container definition
├── Makefile              # auto-discovers kernel/*.c and kernel/*.asm
├── tools/
│   └── screenshot.sh     # boot headless, capture the framebuffer to a PNG
├── docs/
│   └── SETUP.md          # toolchain + build/run notes
├── bootloader/
│   └── boot.asm          # MBR: E820, VBE mode set, GDT, PM transition
└── kernel/
    ├── linker.ld         # loads at 0x10000, exports __bss_start/_end, __kernel_end
    ├── kernel_entry.asm  # long-mode trampoline, BSS zero, call kernel_main
    ├── kernel.c          # boot sequence, shell
    │
    ├── types.h           # u8/u16/u32/u64, bool, attribute macros
    ├── io.h              # port I/O, control registers, invlpg
    ├── string.{h,c}      # memset/memcpy/memcmp/strlen + numeric formatters
    ├── console.{h,c}     # unified framebuffer + VGA + serial output
    │
    ├── idt.{h,c}         # IDT table, dispatch, panic, irq_register
    ├── idt_stubs.asm     # 256 ISR stubs, common save/restore
    ├── pic.{h,c}         # 8259A remap, mask/unmask/EOI
    ├── pit.{h,c}         # channel-0 timer
    ├── keyboard.{h,c}    # PS/2 scancode set 1, ring buffer
    │
    ├── pmm.{h,c}         # E820 + bitmap frame allocator
    ├── vmm.{h,c}         # 4-level paging, map/unmap/translate
    ├── kmalloc.{h,c}     # first-fit heap
    │
    ├── sched.{h,c}       # preemptive round-robin scheduler
    ├── sched_switch.asm  # context-switch register save/restore
    ├── sync.{h,c}        # mutex, semaphore, condvar
    │
    ├── ata.{h,c}         # PIO ATA driver (primary slave)
    ├── fat.{h,c}         # read-only FAT12
    │
    ├── fb.{h,c}          # linear-framebuffer drawing primitives
    ├── fbcon.{h,c}       # framebuffer text console
    └── font8x8.h         # 8x8 bitmap font (ASCII 0x00..0x7F)
```

Add a new `.c` or `.asm` under `kernel/` and the Makefile picks it up
automatically — no Makefile edits needed for routine additions.

## Memory layout

Physical:

| Range                    | Contents                                    |
| ------------------------ | ------------------------------------------- |
| `0x00000000–0x0009FFFF`  | Low memory (IVT, BDA, etc.) — reserved      |
| `0x00007C00`             | Bootloader entry (MBR)                      |
| `0x00009000`+            | E820 entry count + 24-byte entries          |
| `0x00009700`             | Framebuffer descriptor handed to the kernel |
| `0x00009800`             | VBE mode-info scratch block                 |
| `0x000B8000`             | Legacy VGA text framebuffer                 |
| `0x00010000`             | Kernel `_start`                             |
| `0x00010000–__kernel_end`| Kernel image                                |
| `__kernel_end`+          | PMM bitmap                                  |
| Above                    | Free frames                                 |
| `~0xFD000000`            | VESA linear framebuffer (MMIO, see boot.asm)|

Virtual (after `vmm_init`):

| Range                    | Contents                                    |
| ------------------------ | ------------------------------------------- |
| `0x0000000000000000`+    | Identity map of physical RAM (2 MiB pages)  |
| `0x0000000200000000`+    | Kernel heap (4 KiB pages, demand-grown)     |
| `~0xFD000000`            | Framebuffer, identity-mapped (4 KiB pages)  |

## Shell commands

| Command      | What it does                                           |
| ------------ | ------------------------------------------------------ |
| `help`       | List commands                                          |
| `ticks`      | Print PIT tick count since boot                        |
| `mem`        | Print frame counts + heap used/free                    |
| `tasks`      | Scheduler stats + mutex invariant check                |
| `prod`       | Producer/consumer (semaphore) queue stats              |
| `ls`         | List the FAT12 root directory                          |
| `cat <file>` | Print a file from the FAT12 disk                       |
| `halt`       | `cli; hlt` loop — clean stop                           |

## License

See `LICENSE`.
