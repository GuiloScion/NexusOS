# Contributing to NexusOS

Contributions are welcome, to the kernel and to the book in `docs/book` alike.

## Reporting a problem

Open an issue on the [issue tracker](https://github.com/GuiloScion/NexusOS/issues).

**For the kernel**, please include:

- what you did, what you expected, and what happened
- your toolchain (compiler and version, assembler, host OS)
- whether you were running under QEMU or on real hardware, and if real
  hardware, the machine
- the serial log if you have one, or a photograph of the panic screen if you do
  not

**For the book**, please include the chapter and the listing or section number.
Reports that a step does not work as written are especially useful — if you
followed a chapter and your kernel did not do what the text said it would, that
is a defect in the book and I want to know about it.

## Suggesting a change

Open an issue describing the change before opening a pull request for anything
substantial, so we can agree on the approach first. Small fixes — typos,
broken listings, corrected line numbers — can go straight to a pull request.

## Pull requests

- Branch from `main` and keep each pull request to one logical change.
- Match the surrounding style. The kernel is C and NASM syntax assembly; the
  book is Markdown.
- Say in the description what you tested and how. For kernel changes, state
  whether you tested under QEMU, on real hardware, or both.
- Kernel changes that affect the behaviour described in a chapter should update
  that chapter in the same pull request.

## Working through the book

The listing line numbers in each caption refer to the source at the
`v1.0-book` tag. If you have cloned a later commit and the numbers no longer
match, either check out that tag or treat the file and function names in the
caption as the authoritative pointer.

## Getting help

If you are stuck on a chapter, open an issue with the `question` label rather
than a bug report. Questions about a step that did not work are welcome and
help improve the book for the next reader.

## Licensing

The kernel source and the build pipeline are MIT licensed. The book manuscript
in `docs/book` is CC BY 4.0. By contributing you agree that your contribution
is licensed under whichever of these applies to the files you changed.

## Conduct

Be civil and assume good faith. Harassment of any kind is not acceptable and
will result in a contributor being blocked.
