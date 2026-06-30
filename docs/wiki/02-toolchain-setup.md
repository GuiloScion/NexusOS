# 2. Your toolchain and first boot

[← Introduction](01-introduction.md) · [Home](README.md)

Before writing any OS code, you need tools to assemble, compile, link, and run
it. Good news: you don't need a special cross-compiler for x86-64; your normal
host tools work, as long as you tell them *not* to assume an operating system.

## The tools

| Tool                  | Job                                                        |
| --------------------- | ---------------------------------------------------------- |
| `nasm`                | Assembler — turns `.asm` into machine code                 |
| `gcc`                 | C compiler (used in *freestanding* mode)                   |
| `ld`                  | Linker — places code/data at the right addresses           |
| `objcopy`             | Extracts a flat binary from the linked ELF                 |
| `qemu-system-x86_64`  | Emulates a PC so you can boot your OS instantly            |
| `mtools`              | Builds a FAT disk image without root (for later chapters)  |
| `gdb`                 | Debugger (optional but invaluable)                         |

### Installing

- **Linux (Debian/Ubuntu):** `sudo apt install nasm gcc binutils make mtools qemu-system-x86 gdb`
- **macOS:** `brew install nasm qemu mtools x86_64-elf-binutils`
- **Windows:** use **WSL** (Ubuntu) and the Linux instructions — by far the
  smoothest path. (See NexusOS's [SETUP.md](../SETUP.md).)

## "Freestanding" — the key idea

Normally `gcc hello.c` produces a program that runs *on* an OS: it links against
libc and assumes `main`, a stack, a heap, etc. We have none of that. We compile
**freestanding**: no standard library, no startup files, no assumptions. The
NexusOS flags (from its `Makefile`) say exactly this:

```
-ffreestanding   # no hosted environment / standard library
-fno-pic         # absolute addresses (we control where things live)
-fno-stack-protector -fno-builtin
-mno-red-zone    # the SysV red zone is unsafe with interrupts
-mno-mmx -mno-sse -mno-sse2   # don't use vector regs (not set up yet)
-m64 -std=c11
```

You don't have to memorize these, just know that they tell the compiler *"there
is no OS here; emit plain 64-bit code that touches nothing it shouldn't."*

## How a build comes together

An OS image is usually built like this:

1. **Assemble the bootloader** to a flat binary (`nasm -f bin boot.asm`).
2. **Compile** each kernel `.c`/`.asm` to object files.
3. **Link** them with a *linker script* that says where the kernel loads in
   memory (NexusOS: `0x10000`, see `kernel/linker.ld`).
4. **objcopy** the linked ELF into a flat binary.
5. **Concatenate** bootloader + kernel into one disk image.

NexusOS's `Makefile` does all of this and auto-discovers any `.c`/`.asm` you add
under `kernel/`. To build and run it:

```sh
make rungui   # GUI in a window
make run      # headless, serial only
```

## Your first boot: a 512-byte "OK"

The smallest possible OS is a **boot sector**: 512 bytes the BIOS loads and runs.
Here's one that prints `OK` and halts — your "hello world" of bare metal:

```asm
[BITS 16]            ; the CPU starts in 16-bit "real mode"
[ORG 0x7C00]         ; the BIOS loads us here
    mov ah, 0x0E     ; BIOS teletype: print AL
    mov al, 'O'
    int 0x10
    mov al, 'K'
    int 0x10
.hang:
    hlt
    jmp .hang

times 510 - ($ - $$) db 0   ; pad to 510 bytes
dw 0xAA55                    ; the boot signature the BIOS looks for
```

Build and run it:

```sh
nasm -f bin boot.asm -o boot.bin
qemu-system-x86_64 -drive format=raw,file=boot.bin
```

A window opens and prints `OK`. **You just wrote and ran an operating system's
first code on a (virtual) bare machine.** Everything from here is adding layers.

If you want to understand *why* `[BITS 16]`, `0x7C00`, and `0xAA55` are what they
are, that's exactly the next chapter.

---

## End of the free preview

This is where this online preview ends. The remaining eleven chapters cover, in order: how a PC boots in real mode (Chapter 3), the mode-switch ritual from 16-bit through 32-bit to 64-bit long mode (Chapter 4), your first C kernel with two output channels and a linker script (Chapter 5), interrupts and a panic-dumping IDT (Chapter 6), the memory manager (Chapter 7), a preemptive scheduler with mutexes and semaphores (Chapter 8), a PIO ATA driver and FAT12 (Chapter 9), framebuffer graphics (Chapter 10), a compositing window manager (Chapter 11), the honest chapter on real-hardware debugging (Chapter 12), and a roadmap of what to build next (Chapter 13). Plus a glossary with chapter cross-references and an appendix containing the full source of the Makefile, linker script, bootloader, and kernel entry stub.

The complete book is on Leanpub at **[leanpub.com/build-your-own-os](https://leanpub.com/build-your-own-os)**. The free Leanpub sample contains exactly the same scope as this preview (Preface plus Chapters 1 and 2) plus a colophon and closing chapter map, so you can grab it there too if you'd rather have the typeset PDF.

The kernel source the book's listings refer to is in the rest of this repository, pinned to the [`v1.0-book`](https://github.com/GuiloScion/NexusOS/releases/tag/v1.0-book) tag.

[← Introduction](01-introduction.md) · [Home](README.md)
