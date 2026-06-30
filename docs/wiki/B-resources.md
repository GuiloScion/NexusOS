# Resources & further reading

The references that make OS development tractable. You don't need all of these —
but when you're stuck, one of them has the answer.

## The essential reference

- **The OSDev Wiki** (`wiki.osdev.org`) — the community knowledge base for hobby
  OS development. Articles on the bootloader, GDT, IDT, paging, ATA, VBE, APIC,
  and almost everything in this guide. Start here when implementing any new
  subsystem; cross-check its examples against the hardware manuals.
- **The OSDev forums** — searchable archives of nearly every mistake you'll make,
  already debugged by someone else.

## The authoritative manuals

When the wiki and reality disagree, the manuals win:

- **Intel® 64 and IA-32 Architectures Software Developer's Manual (SDM)** — the
  definitive word on x86/x86-64: instructions, modes, paging, interrupts. Huge,
  but Volume 3 (System Programming) is what you'll live in.
- **AMD64 Architecture Programmer's Manual (APM)** — AMD's equivalent; often
  clearer on long-mode specifics.
- **Device datasheets** — the 8259A PIC, 8254 PIT, 8042 PS/2 controller, and the
  ATA/ATAPI spec. Short, and they remove all guesswork about port behavior.

## Books & courses

- **Operating Systems: Three Easy Pieces** (free online) — the best conceptual
  grounding in OS *ideas* (processes, memory, concurrency, filesystems). Pair its
  theory with this wiki's practice.
- **xv6** (MIT) — a small, clean, well-commented teaching OS with an accompanying
  book. Reading xv6 alongside your own code is enormously clarifying. (It's RISC-V
  / older x86, but the structure transfers.)
- **"The little book about OS development"** — a short, friendly walkthrough of
  early bring-up on x86.

## Tools

- **QEMU** — your primary test machine. `-serial stdio`, `-d int,cpu_reset` (log
  interrupts and resets), and the monitor (`screendump`, `info registers`,
  `input-send-event`) are invaluable.
- **GDB** — attach to QEMU (`-s -S`) to single-step the kernel. The book's
  Chapter 12 covers the full real-hardware debugging toolkit.
- **Bochs** — a slower emulator with an even more detailed internal debugger;
  great for diagnosing triple faults and mode-transition bugs QEMU glosses over.
- **NASM / GCC / LD / objcopy / mtools** — the build toolchain (see
  [Chapter 2](02-toolchain-setup.md)).

## Read the reference OS

The most useful resource for *this* guide is the code it's built on. Every
chapter points at the file that implements it; reading
[NexusOS](https://github.com/GuiloScion/NexusOS#readme) end to end — bootloader, kernel, window manager — is a
complete, working example you can run, modify, and break.

[← Back to Home](README.md)
