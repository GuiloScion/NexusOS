# NexusOS documentation

**NexusOS** is a small x86-64 operating system written entirely from scratch —
custom bootloader, 64-bit kernel, preemptive scheduler, memory management, a
FAT12 filesystem, and a compositing window manager — that boots in QEMU and on
real hardware.

> 📄 **[Download the entire guide as a single PDF](pdf/nexusos.pdf)** — the whole
> site (course + reference) in one file.

This site has two halves:

## 📚 Build Your Own OS — the course

A complete, beginner-friendly course on writing an operating system from the
first boot instruction to a graphical desktop on real hardware, taught through
NexusOS as the live worked example.

→ **[Start the course](wiki/README.md)**

Highlights:

- [How a PC boots](wiki/03-how-a-pc-boots.md) and [from 16-bit to 64-bit](wiki/04-protected-long-mode.md)
- [Interrupts](wiki/06-interrupts.md), [memory management](wiki/07-memory-management.md), and a [scheduler](wiki/08-multitasking.md)
- [Graphics](wiki/10-framebuffer-graphics.md) and a [window manager](wiki/11-window-manager.md)
- [Running on real hardware](wiki/12-real-hardware.md) (and how to debug)

## 🔧 Project reference

Documentation for the NexusOS codebase itself:

- **[Architecture](ARCHITECTURE.md)** — how the OS is put together, end to end
- **[Running on real hardware](HARDWARE.md)** — flashing, BIOS settings, caveats
- **[Development setup](SETUP.md)** — toolchain, building, running, debugging

The source lives on [GitHub](https://github.com/GuiloScion/NexusOS).
