# NexusOS

A small x86-64 operating system, written from scratch. No GRUB, no Limine, no
tutorial framework — the bootloader, kernel, build system, and every line of
code are original.

## Status

Boots on QEMU. After hardware init the kernel drops you into an interactive
shell over serial and VGA.

### What works

- **Two-stage boot.** Custom 512-byte MBR bootloader transitions real → protected →
  long mode, collects the BIOS E820 memory map, and loads the kernel.
- **Long-mode kernel** linked at `0x10000`, BSS zeroed at entry.
- **Output** on both VGA text mode (`0xB8000`) and serial (COM1, 115200 8N1).
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
- **Interactive shell.** `help`, `ticks`, `mem`, `halt`.

### What's next

In rough order of usefulness:

1. Cooperative or preemptive scheduler (tasks, context switch).
2. Syscall interface (`syscall`/`sysret`).
3. VFS layer + a real filesystem driver (FAT12/16 is the gentle entry).
4. User mode — ring 3, separate address spaces, ELF loader.
5. SMP, ACPI parsing, APIC.

## Requirements

Toolchain (installed automatically if you use the devcontainer):

- `nasm`
- `gcc` (any reasonably modern host gcc — we don't need a cross-compiler since
  we target ELF64 and pass `-ffreestanding -m64 -mno-red-zone`)
- `ld`, `objcopy` (binutils)
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
sudo apt install nasm gcc binutils qemu-system-x86 gdb
make run
```

You should see:

```
NexusOS booting...
[ok] console
[ok] idt
[ok] pic
[ok] pit
[ok] keyboard
[ok] pmm  (XXX MiB usable)
[ok] vmm
[ok] kmalloc
nexus>
```

`Ctrl-A x` exits QEMU.

### Debugging

```sh
make debug
```

Launches QEMU paused (`-s -S`) and attaches GDB with the kernel ELF loaded.
A breakpoint is set on `kernel_main`. `continue` to start.

## File layout

```
.
├── .devcontainer/        # Codespaces / VS Code container definition
├── Makefile              # auto-discovers kernel/*.c and kernel/*.asm
├── bootloader/
│   └── boot.asm          # MBR, GDT, PM transition, E820 collection
└── kernel/
    ├── linker.ld         # loads at 0x10000, exports __bss_start/_end, __kernel_end
    ├── kernel_entry.asm  # long-mode trampoline, BSS zero, call kernel_main
    ├── kernel.c          # boot sequence, shell
    │
    ├── types.h           # u8/u16/u32/u64, bool, attribute macros
    ├── io.h              # port I/O, control registers, invlpg
    ├── string.{h,c}      # memset/memcpy/memcmp/strlen + numeric formatters
    ├── console.{h,c}     # unified VGA + serial output
    │
    ├── idt.{h,c}         # IDT table, dispatch, panic, irq_register
    ├── idt_stubs.asm     # 256 ISR stubs, common save/restore
    ├── pic.{h,c}         # 8259A remap, mask/unmask/EOI
    ├── pit.{h,c}         # channel-0 timer
    ├── keyboard.{h,c}    # PS/2 scancode set 1, ring buffer
    │
    ├── pmm.{h,c}         # E820 + bitmap frame allocator
    ├── vmm.{h,c}         # 4-level paging, map/unmap/translate
    └── kmalloc.{h,c}     # first-fit heap
```

Add a new `.c` or `.asm` under `kernel/` and the Makefile picks it up
automatically — no Makefile edits needed for routine additions.

## Memory layout

Physical:

| Range                    | Contents                                    |
| ------------------------ | ------------------------------------------- |
| `0x00000000–0x0009FFFF`  | Low memory (IVT, BDA, etc.) — reserved      |
| `0x00007C00`             | Bootloader entry (MBR)                      |
| `0x00009000–0x00009008`  | E820 entry count + entries                  |
| `0x000B8000`             | VGA text framebuffer                        |
| `0x00010000`             | Kernel `_start`                             |
| `0x00010000–__kernel_end`| Kernel image                                |
| `__kernel_end`+          | PMM bitmap                                  |
| Above                    | Free frames                                 |

Virtual (after `vmm_init`):

| Range                    | Contents                                    |
| ------------------------ | ------------------------------------------- |
| `0x0000000000000000`+    | Identity map of physical RAM (2 MiB pages)  |
| `0x0000000200000000`+    | Kernel heap (4 KiB pages, demand-grown)     |

## Shell commands

| Command  | What it does                                               |
| -------- | ---------------------------------------------------------- |
| `help`   | List commands                                              |
| `ticks`  | Print PIT tick count since boot                            |
| `mem`    | Print total / used / free frame counts                     |
| `halt`   | `cli; hlt` loop — clean stop                               |

## License

See `LICENSE`.
