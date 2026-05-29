# 3. How a PC boots: BIOS, real mode, and the bootloader

[← Toolchain](02-toolchain-setup.md) · [Home](README.md) · [Next: Protected & long mode →](04-protected-long-mode.md)

## What happens when you press power

On a legacy/BIOS PC, the sequence is:

1. The CPU powers on in **16-bit real mode**, the ancient mode of the original
   1978 8086, for backward compatibility. Only 1 MiB of memory is addressable,
   and there's no memory protection.
2. The **BIOS** (firmware on the motherboard) runs: it tests hardware, then
   looks for a bootable device.
3. It reads the **first 512-byte sector** (the *boot sector* / *MBR*) of that
   device into memory at address **`0x7C00`** and jumps there — *if* the last
   two bytes are the signature `0x55 0xAA`.
4. Your code is now running. You have 512 bytes and the BIOS's help (via
   *interrupts*) to bootstrap everything else.

That's why our boot sector had `[ORG 0x7C00]` (where it's loaded), `[BITS 16]`
(real mode), and `dw 0xAA55` at the end (the signature).

## Real mode and "segment:offset"

In real mode, addresses are formed as `segment * 16 + offset`. The segment
registers (`cs`, `ds`, `es`, `ss`) hold the segment. It's clunky, but you mostly
just set the segments to 0 and use offsets. NexusOS's bootloader does exactly
that at the top of `bootloader/boot.asm`:

```asm
    cli              ; no interrupts while we set up
    xor ax, ax
    mov ds, ax       ; ds = es = ss = 0
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00   ; stack grows down from our load address
    sti
```

## The BIOS is a free toolkit (while it lasts)

In real mode you can call BIOS services with the `int` instruction. The two you
need at boot:

- **`int 0x10`** — video. With `ah=0x0E`, it prints the character in `al`
  (teletype). That's how the bootloader prints "Booting NexusOS...".
- **`int 0x13`** — disk. Reads sectors from the boot device into memory.
- **`int 0x15, ax=0xE820`** — returns the **memory map** (which physical address
  ranges are usable RAM vs. reserved). The kernel needs this later, and it can
  *only* be obtained in real mode — so the bootloader grabs it now and stashes
  it in memory for the kernel.

> The moment you leave real mode (next chapter), the BIOS is gone. Anything
> you need from it, the memory map, a video mode, you must collect *first*.

## What a bootloader actually does

512 bytes is tiny, so the bootloader's job is narrow: **set up just enough, then
load and jump to the real kernel.** NexusOS's MBR:

1. Saves the boot drive number the BIOS gave it.
2. Collects the **E820 memory map** (stored at physical `0x9000`).
3. Optionally sets a **graphics mode** via the video BIOS (we'll cover this in
   the graphics chapter).
4. **Loads the kernel** from disk to `0x10000`.
5. Switches the CPU into protected/long mode and jumps to the kernel.

### Loading the kernel from disk

The classic way is `int 0x13, ah=0x02` (CHS addressing). It works in QEMU, but
**on real hardware booting from USB it often hangs**, because CHS geometry is a
fiction for modern devices. NexusOS uses the **LBA extensions** (`int 0x13,
ah=0x42`) with a *Disk Address Packet* instead — a flat "give me N sectors
starting at LBA X" request that every USB-boot BIOS supports:

```asm
    mov si, kernel_dap   ; points to the packet below
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13

kernel_dap:
    db 0x10              ; packet size
    db 0
    dw 127               ; sectors to read
    dw 0x0000            ; destination offset
    dw 0x1000            ; destination segment -> 0x1000:0 = 0x10000
    dq 1                 ; start at LBA 1 (sector 0 is the MBR)
```

This is a real lesson the NexusOS project learned the hard way on an actual PC —
see [HARDWARE.md](../HARDWARE.md). When you test on metal, prefer LBA.

## Where things live in low memory

A handy mental map of the first megabyte while booting:

| Address       | What's there                          |
| ------------- | ------------------------------------- |
| `0x07C00`     | Your boot sector (loaded by BIOS)     |
| `0x09000`     | (NexusOS) E820 memory map we saved    |
| `0x10000`     | Where we load the kernel              |
| `0xB8000`     | VGA text-mode screen memory           |

## Try it

Extend your "OK" boot sector to:

1. Print a longer message with a loop (`int 0x10` in a loop over a string).
2. Read a second sector from the disk with `int 0x13` and jump to it.

That second step, load more code and jump to it, *is* the heart of a
bootloader. Once you can do that reliably, you're ready to leave the cramped
16-bit world behind.

[← Toolchain](02-toolchain-setup.md) · [Home](README.md) · [Next: Protected & long mode →](04-protected-long-mode.md)
