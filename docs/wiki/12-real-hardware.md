# 12. Running on real hardware (and how to debug)

[← Window manager](11-window-manager.md) · [Home](README.md) · [Next: Beyond →](13-beyond.md)

Your OS boots beautifully in QEMU. Now put it on a real PC. Brace yourself: this
is where you learn that **the emulator was lying to you** — gently smoothing over
a hundred messy realities that actual firmware does not. This chapter is the most
honest one in the wiki.

## Getting it onto a machine

NexusOS is a **BIOS/legacy** OS, so:

1. **Build a raw disk image** (`build/os.bin` = MBR + kernel).
2. **Write it raw to a USB stick** — *not* as a file. Use Rufus in "DD Image
   mode", balenaEtcher, or `dd`. This puts your MBR in sector 0 of the stick.
3. **Configure firmware:** enable **Legacy / CSM** boot (a pure-UEFI machine
   can't run a BIOS MBR), and enable **USB Legacy Support** (so a USB keyboard is
   emulated as PS/2 and your driver sees it). Boot from the stick.

Full step-by-step (with screenshots of the failure modes) lives in
[HARDWARE.md](../HARDWARE.md).

## The five things that will break (they did for NexusOS)

Every one of these worked perfectly in QEMU and failed on a real ASUS desktop.
They're worth internalizing because they're representative of *why* real-hardware
bring-up is hard.

1. **CHS disk reads hang.** The bootloader loaded the kernel with the old
   cylinder/head/sector `int 13h` call. On real USB-boot BIOSes, a multi-sector
   CHS read that crosses a track boundary can hang forever. **Fix:** use the
   `int 13h` **LBA extensions** (AH=42h) with a Disk Address Packet — universally
   supported by anything that boots from USB.

2. **The memory map is empty.** The BIOS `int 15h, E820` call returned a map with
   **no usable RAM** — only reserved MMIO regions. The machine obviously has
   memory; the firmware just didn't report it the way we expected. **Fix:** when
   the map has no usable region, fall back to a conservative assumption (NexusOS
   assumes 128 MiB above 1 MiB). See `pmm_init`.

3. **A missing disk hangs the boot.** ATA init probed the legacy IDE port; with a
   SATA/AHCI disk there's nothing there, the bus reads `0xFF`, and the "wait while
   busy" loop spun forever (0xFF has the busy bit set). **Fix:** treat `0xFF` as
   "no device" and bound every hardware wait loop with a timeout.

4. **VBE hangs.** Re-enabling the graphics mode, the firmware **hung inside the
   VBE `int 10h` call** — and you can't time-out a BIOS call that never returns.
   **Fix:** make graphics optional. NexusOS ships a `make text` build that skips
   VBE entirely; it runs perfectly in text mode on that board. (The GUI still
   works on QEMU and on hardware with sane VBE.)

5. **Text-mode backspace printed garbage.** The framebuffer console handled
   backspace, but the *VGA text* console (only used on real text-mode hardware)
   didn't — so backspace printed character `0x08`, a little box glyph. A
   one-line fix, but you only find it on metal.

The meta-lesson: **bound every wait loop, validate every firmware return value,
and make every nice-to-have degrade gracefully.** Defensive code is the
difference between "works in QEMU" and "works."

## Debugging

On real hardware you often have *no output* when something fails early. Build a
toolkit:

- **Serial first.** Mirror all kernel output to the serial port (COM1). On a real
  machine, a serial cable or a PCI/USB serial adapter gives you a log even when
  the screen is dead. In QEMU it's just `-serial stdio`. This is your #1 tool.
- **Stage markers.** When you suspect a hang in early boot (before the console is
  up), print a single character after each stage (`1` after the memory map, `M`,
  `K`, `P`…). The last character you see tells you exactly where it died. NexusOS
  used this to pinpoint a hang to "inside the VBE call."
- **GDB + QEMU.** `make debug` launches QEMU paused (`-s -S`) and attaches GDB
  with symbols and a breakpoint on `kernel_main`. You can single-step the kernel,
  inspect registers, and watch page tables — a luxury you lose on bare metal.
- **`screendump`.** QEMU's monitor can dump the framebuffer to an image without a
  window — perfect for headless/automated checks (`tools/screenshot.sh`).
- **Distinct banners.** Re-flashing a USB stick can *silently fail* (the write
  doesn't take). If two builds share a banner, you can't tell which one booted.
  Change the banner text per build so a stale flash is obvious. NexusOS lost an
  hour to a flaky stick before adding this.

## Try it

- Add a serial-only "panic" path that prints the faulting RIP/CR2 and registers
  (NexusOS's exception handler already does this) — then deliberately dereference
  a null pointer and read the dump.
- Boot your OS on the oldest spare PC you can find. Old machines with real BIOS
  (and even PS/2 ports!) are the friendliest targets.

## Next

You've got a real OS on real metal. The horizon from here — user programs,
system calls, multiple CPUs, networking, and porting to other architectures — is
[Chapter 13](13-beyond.md).

[← Window manager](11-window-manager.md) · [Home](README.md) · [Next: Beyond →](13-beyond.md)
