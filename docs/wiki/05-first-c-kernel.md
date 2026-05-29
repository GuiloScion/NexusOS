# 5. Your first C kernel: freestanding code and output

[← Protected & long mode](04-protected-long-mode.md) · [Home](README.md) · [Next: Interrupts →](06-interrupts.md)

You've reached `kernel_main()` in 64-bit mode. From here you write C — but a
special kind: **freestanding** C, with no standard library. This chapter covers
how that code is built and linked, and how to get text on the screen so you can
*see* what your kernel is doing.

## There is no libc — you are the library

On bare metal there's no `printf`, no `malloc`, no `strlen`. If you want them,
you write them. NexusOS has a tiny `kernel/string.c` with `memset`, `memcpy`,
`memcmp`, `strlen`, and number-to-string helpers — that's the entire "standard
library." Keep these minimal; add functions only when you need them.

The compiler can still *assume* a few of these exist (it may emit a call to
`memcpy` for a struct copy), which is why we provide them and pass
`-fno-builtin`.

## The linker script: where your kernel lives

The bootloader loaded the kernel to physical `0x10000`, so the linker must place
code expecting to run there. A linker script (`kernel/linker.ld`) controls the
layout:

```ld
ENTRY(_start)
SECTIONS {
    . = 0x10000;            /* kernel loads/runs here */
    .text   : { *(.text*) }
    .rodata : { *(.rodata*) }
    .data   : { *(.data*) }
    __bss_start = .;
    .bss    : { *(.bss*) *(COMMON) }
    __bss_end = .;
    __kernel_end = .;
}
```

- `_start` (in the assembly entry stub) must come **first** so it lands exactly
  at `0x10000`.
- `__bss_start`/`__bss_end` let the entry code zero uninitialized globals.
- `__kernel_end` tells the memory manager where the kernel image ends, so it
  knows where free memory begins.

After linking, `objcopy -O binary` strips the ELF wrapper to a flat binary that
the bootloader can load verbatim.

## Output #1: VGA text mode

The simplest screen output is **VGA text mode**: a grid of 80×25 character cells
living at physical address `0xB8000`. Each cell is two bytes — an ASCII code and
a color attribute. Write there and it appears instantly:

```c
volatile uint16_t *vga = (uint16_t *)0xB8000;
vga[0] = (uint16_t)'H' | (0x0F << 8);   // 'H', white-on-black
```

NexusOS wraps this in `kernel/console.c` with a cursor, newline handling, and
scrolling (when you reach the bottom, shift all rows up by one).

## Output #2: the serial port

VGA is great, but when you run headless (or want a *log* you can capture from
QEMU), the **serial port** (COM1, I/O port `0x3F8`) is invaluable. You configure
it once (baud rate, 8N1) and then poll a status bit before sending each byte:

```c
static void serial_putc(char c) {
    while (!(inb(0x3F8 + 5) & 0x20)) { }  // wait until transmit buffer empty
    outb(0x3F8, c);
}
```

Run QEMU with `-serial stdio` and your kernel's output appears in your terminal.
This is the single most useful debugging tool early on — far easier than reading
the screen. NexusOS's `console_putc` writes to **both** serial and the screen,
so you always have a log.

## Port I/O: `inb` / `outb`

Devices like the serial port and (later) the timer and keyboard are controlled
through **I/O ports**, accessed with the `in`/`out` instructions. Wrap them in
inline assembly once (`kernel/io.h`):

```c
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t r; __asm__ volatile ("inb %1, %0" : "=a"(r) : "Nd"(port)); return r;
}
```

You'll use these constantly.

## A minimal `kernel_main`

```c
void kernel_main(void) {
    console_init();
    console_puts("Hello from the kernel!\n");
    for (;;) __asm__ volatile ("hlt");   // idle forever
}
```

Build, `make run`, and you should see your greeting on serial and screen. That's
a working 64-bit kernel printing output — a real milestone.

## A good habit: print your progress

As you add subsystems, print a line as each comes up:

```
[boot] console ready
[boot] idt installed
[pmm] total = 255 MiB, free = 254 MiB
```

This "boot log" is exactly how NexusOS reports progress, and when something
hangs, the *last line printed* tells you which subsystem to suspect. Cheap,
priceless.

Next we'll make the kernel respond to events — the timer ticking, keys being
pressed — which means handling **interrupts**.

[← Protected & long mode](04-protected-long-mode.md) · [Home](README.md) · [Next: Interrupts →](06-interrupts.md)
