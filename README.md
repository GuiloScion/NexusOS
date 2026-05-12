# NexusOS

A from-scratch x86-64 operating system written in Assembly and C.

## Architecture

- **Target**: x86-64 (Intel/AMD)
- **Bootloader**: Custom BIOS bootloader
- **Entry Point**: 16-bit real mode → 32-bit protected mode → 64-bit long mode

## Prerequisites

- `nasm` - Netwide Assembler for bootloader
- `gcc` - Cross-compiler for x86-64
- `ld` - GNU Linker
- `qemu-system-x86_64` - Emulator for testing
- `make` - Build automation
