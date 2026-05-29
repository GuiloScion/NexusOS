# 6. Interrupts: the IDT, exceptions, the PIC, the timer, and the keyboard

[← First C kernel](05-first-c-kernel.md) · [Home](README.md) · [Next: Memory management →](07-memory-management.md)

So far the kernel runs straight through. But a real OS must **react** to things:
a key is pressed, a timer fires, the CPU hits an error. The mechanism for all of
this is the **interrupt** — the CPU stops what it's doing, jumps to a handler you
registered, then resumes. This chapter wires that up.

## Two kinds of interrupts

- **Exceptions** — the CPU itself raises these on errors: divide-by-zero, page
  fault (bad memory access), general protection fault, etc. Vectors 0–31.
- **Hardware interrupts (IRQs)** — devices raise these: the timer (IRQ0), the
  keyboard (IRQ1), the disk, the mouse. They're delivered through a chip called
  the PIC.

Both are dispatched through the same table: the **IDT**.

## The IDT (Interrupt Descriptor Table)

The IDT is an array of 256 entries; entry *N* says "when interrupt *N* fires,
jump here." You fill it in, then load it with `lidt`. NexusOS builds all 256
entries in `kernel/idt.c`.

Each entry points at a small assembly **stub**. Why assembly? Because on entry
you must save the CPU registers before running C (C would clobber them), and
some interrupts push an error code while others don't — the stubs normalize all
that, then call a single C dispatcher. NexusOS generates 256 stubs in
`kernel/idt_stubs.asm` that all funnel into one handler.

### Handling exceptions = your first debugger

When your kernel has a bug — dereferences a bad pointer, divides by zero — the
CPU raises an exception. If you have no handler, it escalates to a
**triple-fault** and the machine reboots (you'll see QEMU restart). With an IDT
in place, you can instead **catch** it and print diagnostics. NexusOS's exception
handler dumps the instruction pointer (`RIP`), the faulting address (`CR2`, for
page faults), and every register, then halts:

```
PANIC: exception 14 (page fault)
RIP=0x... CR2=0x... RAX=0x... ...
```

That panic dump turns "the machine mysteriously rebooted" into "I dereferenced
null at this line." It's worth building early.

## The PIC: routing device interrupts

The legacy **8259A PIC** (a pair of chips) routes the 16 hardware IRQ lines to
CPU interrupt vectors. Two things you must do:

1. **Remap it.** By default the PIC delivers IRQs on vectors 0–15 — which
   *collide* with the CPU's exception vectors. Remap the IRQs to vectors
   `0x20–0x2F` so they don't clash. (`pic_init` in `kernel/pic.c`.)
2. **Acknowledge (EOI).** After handling an IRQ, send an "end of interrupt" so
   the PIC will deliver the next one. Forget this and interrupts stop after the
   first.

You can also **mask** (disable) individual IRQs until you have a driver ready
for them.

## The timer (PIT) — your heartbeat

The **Programmable Interval Timer** can be set to fire IRQ0 at a fixed rate.
NexusOS programs it for 100 Hz (`kernel/pit.c`); each tick increments a counter
and, later, drives the scheduler's preemption. A steady timer interrupt is the
heartbeat of a multitasking OS — it's what lets the kernel take the CPU back
from a running task.

```c
void pit_init(uint32_t hz) {
    uint32_t divisor = 1193182 / hz;   // the PIT's base frequency
    outb(0x43, 0x36);                  // channel 0, rate generator
    outb(0x40, divisor & 0xFF);
    outb(0x40, divisor >> 8);
}
```

## The keyboard

The PS/2 keyboard raises IRQ1 each time a key is pressed or released. The handler
reads a **scancode** from port `0x60`, translates it to a character (via a
lookup table for the US layout), tracks modifiers (Shift, Caps Lock), and pushes
the result into a ring buffer that the shell reads. See `kernel/keyboard.c`.

Key subtlety: the handler must be **short**. It reads the byte, updates state,
and returns — heavy work doesn't belong in an interrupt handler.

## The flow, end to end

```
key pressed
  → PIC raises IRQ1 → CPU jumps to IDT entry 0x21
  → assembly stub saves registers, calls the C handler
  → handler reads scancode (port 0x60), pushes char to ring buffer, sends EOI
  → stub restores registers, returns (iret)
  → later, the shell reads the char from the ring buffer
```

Once interrupts work, your kernel is *alive* — it ticks, it responds to keys.
The catch: interrupts can fire at any moment, even in the middle of your code.
That introduces **concurrency**, which we'll tame with a scheduler and locks
(Chapter 8). But first, the thing every subsystem depends on: memory.

[← First C kernel](05-first-c-kernel.md) · [Home](README.md) · [Next: Memory management →](07-memory-management.md)
