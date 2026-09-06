# Build Your Own Operating System — source

This directory holds the source of [*Build Your Own Operating System*](https://leanpub.com/), the book that ships alongside NexusOS as a hands-on companion. The book is available typeset on Leanpub; the source here is openly licensed and you can rebuild it yourself.

## Learning objectives

By working through this book you will build a functioning x86-64 operating
system from nothing, and understand:

- how a machine boots, and what the firmware hands you
- the transition from real mode through protected mode to long mode
- how interrupts and exceptions are dispatched, and how to write handlers
- virtual memory: paging structures, address translation, and a frame allocator
- preemptive multitasking, and why synchronisation primitives are necessary
- reading from disk without a driver stack, and a simple filesystem
- compositing graphics and a windowing system on a linear framebuffer

Prerequisites: comfort with C, willingness to read x86-64 assembly, and a
Linux or WSL toolchain. No prior operating systems knowledge is assumed.

## What's in this directory

| File                  | What it is                                                          |
| --------------------- | ------------------------------------------------------------------- |
| `manuscript.md`       | Title page + preface + Chapters 1–2                                  |
| `ch03-05.md`          | Chapters 3 (booting), 4 (mode switch), 5 (first C kernel)            |
| `ch06-07.md`          | Chapters 6 (interrupts), 7 (memory management)                       |
| `ch08-09.md`          | Chapters 8 (multitasking), 9 (storage)                               |
| `ch10-11.md`          | Chapters 10 (graphics), 11 (window manager)                          |
| `ch12-end.md`         | Ch 12 (real hardware), Ch 13 (what to build next), Glossary, Resources |
| `ch-appx-c.md`        | Appendix C — full source listings (Makefile, linker, boot.asm, entry) |
| `ch-about.md`         | About the author                                                     |
| `build_book.py`       | The PDF builder: 2-pass layout, TOC, running headers, page numbers   |
| `make_cover.py`       | Generates `cover.png` (1800×2700) for Leanpub                        |
| `cover.png`           | Cover artwork                                                        |

## Rebuilding the PDF

Two dependencies (Windows-friendly paths assumed; works on Linux/macOS with minor font tweaks in `build_book.py`):

```sh
pip install reportlab pypdf pillow
python build_book.py
```

The script produces `BuildYourOwnOS.pdf` in this directory. It runs a two-pass layout to compute the TOC page numbers correctly. Total build time is a few seconds.

To regenerate the cover:

```sh
python make_cover.py
```

## A note on listings

The line numbers in each `Listing N.M.` caption refer to the source files **at this git tag**. If you cloned a later commit and the numbers no longer match, either:

- `git checkout v1.0-book` to pin to the book's reference state, or
- treat the function and file names in the captions as the authoritative pointer; the line numbers are approximate.

## License

The book's manuscript, the `.md` files in this directory, plus `cover.png`, is
licensed under [Creative Commons Attribution 4.0 International](LICENSE) (CC BY 4.0). You are free to share, adapt, translate, and build on it, including commercially, provided you give appropriate credit.

The build pipeline (`build_book.py`, `make_cover.py`) is MIT-licensed; reuse it for your own books.

The NexusOS kernel source that the listings refer to, everything outside `docs/book/`, is MIT-licensed; see the root `LICENSE` file.

A typeset PDF is available on [Leanpub](https://leanpub.com/build-your-own-os).
