# Build Your Own Operating System; a hands-on wiki

Welcome! This is a beginner-friendly course on writing an operating system from
scratch. starting from an empty file and ending with a graphical desktop
running on real hardware. Every chapter is backed by a **complete, working
reference OS** ([NexusOS](../../README.md)), so you can always see exactly how
the idea looks in real, original code.

You do **not** need prior OS-development experience. You should be comfortable
reading C and a little assembly, and willing to learn as you go. We explain the
*why* behind each step, not just the *how*.

## How to use this wiki

- **Read in order** the first time, each chapter builds on the last.
- Keep the [NexusOS source](../../) open alongside. When a chapter says
  *"see `kernel/pmm.c`"*, go read it; that's the worked example.
- After each chapter, **build and run** what you have. Watching it boot (or
  crash!) is how you learn.
- Stuck? The [Debugging](12-real-hardware.md#debugging) tips and the
  [Glossary](glossary.md) are there for you.

## The journey

### Part 0 — Getting started
1. [Introduction: what is an OS, and what will we build?](01-introduction.md)
2. [Your toolchain and first boot](02-toolchain-setup.md)

### Part 1 — Booting
3. [How a PC boots: BIOS, real mode, and the bootloader](03-how-a-pc-boots.md)
4. [From 16-bit to 64-bit: protected mode, paging, and long mode](04-protected-long-mode.md)

### Part 2 — A real kernel
5. [Your first C kernel: freestanding code and output](05-first-c-kernel.md)
6. [Interrupts: the IDT, exceptions, the PIC, the timer, and the keyboard](06-interrupts.md)
7. [Memory management: physical frames, paging, and the heap](07-memory-management.md)

### Part 3 — Making it do things at once
8. [Multitasking: a scheduler and synchronization](08-multitasking.md)
9. [Storage: talking to a disk and reading a filesystem](09-storage.md)

### Part 4 — A graphical desktop
10. [Graphics: the framebuffer, fonts, and a console](10-framebuffer-graphics.md)
11. [A window manager: the mouse, a compositor, and windows](11-window-manager.md)

### Part 5 — The real world and beyond
12. [Running on real hardware (and how to debug)](12-real-hardware.md)
13. [Beyond: userspace, syscalls, and the road ahead](13-beyond.md)

Plus: a [Glossary](glossary.md) of the jargon, and [Resources & further
reading](B-resources.md) for when you want to go deeper.

## What you'll have built

By the end you'll understand, and have a reference for, every layer of a small
but real OS:

- a custom bootloader that takes the CPU from 16-bit real mode to 64-bit long mode,
- a kernel with interrupts, a physical + virtual memory manager, and a heap,
- a preemptive scheduler with mutexes and semaphores,
- a disk driver and a filesystem,
- a graphics stack with a compositing window manager and a mouse cursor,
- and the knowledge to boot it on an actual PC.

Let's go → **[Chapter 1: Introduction](01-introduction.md)**.
