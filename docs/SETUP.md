# NexusOS Development Setup

## Prerequisites

You need `nasm`, a host `gcc`/`ld`/`objcopy` (binutils), `qemu-system-x86_64`,
and `mtools` (the FAT12 disk image is built with `mformat`/`mcopy`, no root
required). `gdb` is optional, for `make debug`.

### Ubuntu/Debian
```bash
sudo apt-get install nasm gcc binutils make mtools qemu-system-x86 gdb
```

### macOS (with Homebrew)
```bash
brew install nasm qemu mtools x86_64-elf-binutils
```

### Windows
The build is Linux-oriented; the simplest path is **WSL** (Ubuntu), where the
commands above apply unchanged. A native MSYS2 setup also works:
```bash
pacman -S nasm gcc binutils mtools qemu
```

## Building NexusOS

```bash
make all
```

This will:
1. Assemble the bootloader
2. Compile and link the kernel
3. Combine them into `build/os.bin`
4. Build the FAT12 disk image `build/fat.img` (with a couple of sample files)

## Running NexusOS

```bash
make run
```

QEMU launches with the OS image as the primary disk and the FAT12 image as a
second disk, opening a window with the graphical console. Output is also
mirrored to serial. `Ctrl-A x` exits when running headless.

To capture the framebuffer to a PNG (handy on headless/WSL setups):

```bash
bash tools/screenshot.sh        # -> build/screen.png
```

## Debugging

```bash
make debug
```

This starts QEMU in debug mode and connects GDB.

## Troubleshooting

If `make all` fails:
- Ensure `nasm`, `gcc`, `ld`, and `mtools` are in your PATH
- Try `make clean` then `make all`

If QEMU doesn't start:
- Ensure `qemu-system-x86_64` is installed
- On WSL, a window needs WSLg; otherwise run headless and use
  `tools/screenshot.sh` to capture the framebuffer
