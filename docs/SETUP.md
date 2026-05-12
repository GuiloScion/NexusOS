# Development Setup

## Prerequisites

### Ubuntu/Debian
```bash
sudo apt-get install nasm gcc make qemu-system-x86
sudo apt-get install binutils-x86-64-linux-gnu
```

### macOS (with Homebrew)
```bash
brew install nasm qemu
brew tap Homebrew/homebrew-cask
```

### Windows (MSYS2)
```bash
pacman -S nasm gcc binutils qemu
```

## Building

```bash
make build
```

This will:
1. Assemble the bootloader
2. Compile and link the kernel
3. Combine them into `build/os.bin`

## Running

```bash
make run
```

QEMU will launch with your OS image.

## Debugging

```bash
make debug
```

This starts QEMU in debug mode and connects GDB.

## Troubleshooting

If `make build` fails:
- Ensure `nasm`, `gcc`, and `ld` are in your PATH
- Check that the `build/` directory exists
- Try `make clean` then `make build`

If QEMU doesn't start:
- Ensure `qemu-system-x86_64` is installed
- Check file permissions on `build/os.bin`
