# Build Your Own Operating System — free preview

This was the original draft of *Build Your Own Operating System*, a hands-on book that takes you from a 512-byte BIOS boot sector to a graphical desktop running on real hardware. It is paired with **NexusOS**, the reference kernel you'll find in the rest of this repository.

The polished and complete book is now on Leanpub: **[leanpub.com/build-your-own-os](https://leanpub.com/build-your-own-os)**.

## What's here, and what isn't

This directory contains the first two chapters as a free preview, the same scope as the Leanpub free sample:

1. [Introduction: what is an OS, and what will we build?](01-introduction.md)
2. [Your toolchain and first boot](02-toolchain-setup.md)

Plus the back-of-book reference material that the published book also includes:

- [Glossary](glossary.md) — the jargon of OS development in plain language
- [Resources & further reading](B-resources.md) — the references that make OS development tractable

## What's in the full book

The eleven chapters past Chapter 2 are in the paid Leanpub edition only. Briefly:

| Chapter | Topic |
| ------- | ----- |
| 3 | How a PC boots: BIOS, real mode, and the bootloader |
| 4 | From 16-bit to 64-bit: protected mode, paging, and long mode |
| 5 | Your first C kernel: freestanding code and output |
| 6 | Interrupts: the IDT, exceptions, the PIC, the timer, and the keyboard |
| 7 | Memory management: physical frames, paging, and the heap |
| 8 | Multitasking: a scheduler and synchronization |
| 9 | Storage: talking to a disk and reading a filesystem |
| 10 | Graphics: the framebuffer, fonts, and a console |
| 11 | A window manager: the mouse, a compositor, and windows |
| 12 | Running on real hardware (and how to debug) |
| 13 | What to build next |

The book is ~100 pages, typeset in Sitka Display and Consolas, with line-numbered code listings tied to specific functions in this repository at the [`v1.0-book`](https://github.com/GuiloScion/NexusOS/releases/tag/v1.0-book) tag. It also includes Appendix A (full glossary with chapter cross-references), Appendix B (resources), Appendix C (full source of `Makefile`, `linker.ld`, `boot.asm`, `kernel_entry.asm`), and an About-the-Author page.

The book is on Leanpub at **[leanpub.com/build-your-own-os](https://leanpub.com/build-your-own-os)** with a downloadable free sample (the same content as this preview, plus a colophon and a closing chapter map).

## How to read this preview

Read in order. Each chapter assumes the subsystem from the previous one.

Keep the NexusOS source open alongside; chapter 2 walks you through the toolchain and ends with the smallest possible OS, a 512-byte program that prints `OK` on a virtual machine. By the end of these two chapters you'll have set up your tools and shipped your first booting code.

Stuck? The [Glossary](glossary.md) defines every term the book uses.

Let's go → **[Chapter 1: Introduction](01-introduction.md)**.
