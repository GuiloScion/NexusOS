## Title

Build Your Own Operating System

## Subtitle

A hands-on book, from boot sector to graphical desktop

## Short / one-line description (used in cards and search)

A working x86-64 kernel in 90 pages: bootloader, scheduler, FAT12 driver, window manager — all the way to running on a real PC.

## Long description (the "About the Book" page)

**A small, hands-on book on writing an operating system that boots on an actual computer.**

Most OS-development resources either send you to a graduate textbook (OSTEP) or hand you a tutorial that works perfectly in QEMU and silently fails on real hardware. This one does neither.

*Build Your Own Operating System* is a hands-on companion to **NexusOS**, a ~3,000-line x86-64 reference kernel that:

- boots from a 512-byte BIOS bootloader you can read in one sitting,
- runs in 64-bit long mode with its own GDT and 4-level page tables,
- manages physical and virtual memory through a real PMM/VMM/heap stack,
- preempts tasks at 100 Hz with mutex, semaphore, and condition-variable primitives,
- talks to a disk over PIO ATA and reads a FAT12 filesystem,
- composites a graphical desktop with a draggable terminal window,
- and boots on actual hardware, not just in the emulator.

Each chapter introduces one subsystem in plain language, **inlines the actual code** that makes it work (line numbers match the source tree), and ends with exercises that ask you to *modify* the reference kernel rather than retype 3,000 lines from a blank file.

### What this book is *not*

It is not a doorstop. At ~90 pages it deliberately trades comprehensiveness for momentum. If you want the formal treatment of concurrency or virtual memory at the level of a graduate textbook, OSTEP is excellent and free; read it alongside this one. If you want a pure tutorial that walks you from byte zero, Phil Opp's Rust OS series is the gold standard for Rust, and the *Little Book About OS Development* is a friendly x86 starting point. This book is something between those: a compact path from `[BITS 16]` to a draggable terminal window, paired with a kernel you can clone, boot, modify, and break.

### What makes this book different from the free alternatives

- **Real-hardware war stories.** When NexusOS first booted on an ASUS desktop, the BIOS `E820` call returned a memory map with **no usable RAM**. Then the ATA driver hung waiting on a SATA-only machine, because the legacy IDE port reads `0xFF` (which has the busy bit set). Then VBE hung *inside* the BIOS call, and you cannot time-out a BIOS call that never returns. Each chapter has the specific bug, the specific fix, and the defensive code that survived. Chapter 12 is the most honest chapter in the book.
- **A working kernel, not pseudocode.** xv6 is RISC-V (or older x86) and academic. Phil Opp's series is excellent, but Rust. NexusOS is C and assembly, x86-64, and you can `make rungui` today and modify it.

### What you'll need

Comfort with C and a little assembly. WSL on Windows, or any Linux/macOS shell. No prior bare-metal experience. About 90 pages of focused reading, plus as much time at the keyboard as you choose to give it.

### What you'll have at the end

By Chapter 11, a graphical desktop running in an emulator. By Chapter 12, the same OS running on a real PC, with a panic dump you wrote yourself arriving on a serial cable when something goes wrong. By Chapter 13, a clear picture of what to write next (user mode, syscalls, ELF, SMP, networking, porting) and how to start.

### Free sample

The free sample includes the Preface and Chapters 1–2: enough to read the philosophy of the book, set up your toolchain, and write the smallest real OS — a 512-byte boot sector that prints `OK` on a virtual machine.

---

## Author bio (Leanpub author page)

Short version (one or two sentences, used in cards and search):

> Noah Parsons writes systems software, computational physics tooling, and applied policy research from Newcastle, Wyoming. He maintains MechanicsDSL (a physics compiler with ~13,000 downloads across 67 countries) alongside the x86-64 kernel this book is built around.

Long version (the full Leanpub author bio, ~180 words):

> Noah Parsons writes systems software, computational physics tooling, and applied policy research from Newcastle, Wyoming.
>
> NexusOS, the x86-64 reference kernel this book is built around, sits next to MechanicsDSL — a multi-target physics compiler built on SymPy that targets a dozen code-generation backends (C++, CUDA, OpenMP, Rust, Julia, Fortran, MATLAB, JavaScript, WebAssembly, Arduino, Unity, Modelica). MechanicsDSL is MIT-licensed and has been downloaded roughly 13,000 times across 67 countries, with institutional mirror adoption on bandersnatch, Nexus, and devpi. He is also at work on an analytical model of energy and information transport in boundary-driven nonequilibrium quantum spin chains, in preparation toward submission.
>
> His policy work has appeared in the peer-reviewed *Applied Journal of Economics, Law and Governance* (grid modernization) and in shorter briefs on Medicare drug-price negotiation, the October 2025 federal shutdown, and U.S. industrial competitiveness. In 2025 he served as Director of Civic Innovation at the American Forge Institute (since dissolved).
>
> Find him at [github.com/GuiloScion](https://github.com/GuiloScion) and ORCID [0009-0000-7224-6040](https://orcid.org/0009-0000-7224-6040). NexusOS, MechanicsDSL, and the source for every listing in this book are MIT-licensed; issues and pull requests welcome.

## Categories (Leanpub)

- Computer Programming
- Operating Systems
- Systems Programming
- C / Embedded

## Tags / keywords

`operating-systems`, `kernel-development`, `x86`, `x86-64`, `bare-metal`, `bootloader`, `c-programming`, `assembly`, `qemu`, `nexusos`, `embedded`, `low-level`

## Sample chapters

Mark these as included in the free sample:
- Preface
- Chapter 1: Introduction: what is an OS, and what will we build?
- Chapter 2: Your toolchain and first boot

## Pricing

- **Minimum:** $4.99
- **Suggested:** $14.99
- **Royalty:** Leanpub default (80% to author)

## Cover

Upload `cover.png` (1800 × 2700, dark slate, Sitka Banner Bold title, NexusOS boot-log terminal motif). It's in this directory.

## The book file

Upload `BuildYourOwnOS.pdf` via the "Upload PDF" route. (Do **not** convert to Markua — the typographic and listing-layout decisions in the PDF are intentional and reflowable formats would degrade them.)

## Optional: ISBN

Leanpub will assign one free if you want one. Not required for a digital-only release. Skip for v1, request later if you ever go to print via IngramSpark.

---

## Tag the repo first

Before you click publish, **tag the NexusOS repo with `v1.0-book`** and push the tag:

```sh
git tag -a v1.0-book -m "Companion to Build Your Own Operating System v1"
git push origin v1.0-book
```

The book's listings reference the file *and the line range* in the source tree. A single later commit that moves `pic_init` down ten lines invalidates every Chapter 6 caption. Tagging pins the listings to a known commit; the appendix and the About-the-author page both tell readers to `git checkout v1.0-book` if the line numbers ever fall out of sync. For each new edition of the book, cut a fresh tag (`v1.1-book`, `v2.0-book`) and update the bookward references in the manuscript.

## A note on launch order

Before you click publish:

1. Confirm the GitHub repo (GuiloScion/NexusOS) is public, has the latest commit, and the README links here. The book repeatedly tells the reader to clone it.
2. Open the PDF on a phone and a tablet to confirm code listings remain readable at smaller zoom levels. They are sized for 6×9 print, which is friendly on small screens but not infinite.
3. Decide whether to launch at $0 minimum for the first week to build reviews, then bump. Either is defensible; the $0 route tends to produce more reviews.
4. Post about the launch in r/osdev, the OSDev forums, and Hacker News (Show HN works for this kind of project). The hardware-bug stories from Chapter 12 make a natural short companion blog post that links to the book.
