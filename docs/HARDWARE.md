# Running NexusOS on real hardware

NexusOS is a BIOS/legacy x86-64 OS. With the right firmware settings it boots
natively from a USB stick on a real PC — no emulator. This guide covers the
process and the quirks we hit on actual machines.

> **TL;DR:** build an image, write it raw to a USB stick (DD mode), enable
> **Legacy/CSM** + **USB Legacy Support** in the firmware, and boot from the
> stick. If the screen hangs on the graphical mode, use the **text build**
> (`make text`).

## 1. Build an image

| Command      | Produces                                                       |
| ------------ | -------------------------------------------------------------- |
| `make all`   | Default **GUI** image (`build/os.bin`) — graphical desktop     |
| `make text`  | **Text-only** image — skips VBE for firmware that hangs on it  |

`build/os.bin` is a raw disk image: a 512-byte MBR followed by the kernel.

## 2. Write it to a USB stick

`os.bin` must be written **raw to the whole device** (not copied as a file).

- **Rufus** (Windows): SELECT `os.bin` (rename to `os.img` if the picker
  filters by extension) → it switches to **DD Image mode** → START.
- **balenaEtcher**: Flash from file → `os.img` → select USB → Flash.
- **dd** (Linux/macOS): `sudo dd if=build/os.bin of=/dev/sdX bs=1M conv=fsync`
  (double-check `/dev/sdX` — this erases the device).

> ⚠️ This erases the USB stick. Triple-check the target device.
>
> ⚠️ Flaky/old sticks can write the first sector (the MBR/banner) correctly but
> corrupt later sectors (the kernel), producing a hang *after* the banner. If a
> boot misbehaves and the code is known-good, **try a different stick or USB
> port** before debugging the code.

## 3. Firmware (BIOS/UEFI) settings

NexusOS uses a BIOS MBR bootloader and BIOS interrupts; it cannot boot under
pure UEFI. In firmware setup:

1. **Boot Mode / CSM → enable Legacy / CSM** (turn off "UEFI only"). This is the
   make-or-break setting. Enabling CSM usually disables Secure Boot
   automatically.
2. **Boot Device Control → "Legacy OPROM"** (or "UEFI and Legacy") so the USB
   shows up as a legacy boot entry.
3. **USB Legacy Support → Enabled** (Advanced → USB Configuration). Required for
   a **USB keyboard** to work — the firmware emulates it as PS/2.
4. Boot from the USB (one-time boot menu, often F12/F11/F8/Esc). If the stick is
   listed twice, choose the entry **without** a "UEFI:" prefix.

## 4. What to expect

On boot you'll see the kernel banner, the `[boot]`/`[pmm]`/`[sched]` log, and a
`nexus>` prompt. Type `help` for commands.

| Feature        | On real hardware                                                |
| -------------- | --------------------------------------------------------------- |
| Boot → 64-bit  | ✅ with CSM enabled                                              |
| Keyboard       | ✅ usually, via USB Legacy Support                              |
| Graphical GUI  | ⚠️ only if the video BIOS supports VBE 1024×768; otherwise the kernel falls back to a text console (or use `make text`) |
| Mouse          | ❌ PS/2 only; USB-mouse legacy emulation is unreliable          |
| `ls` / `cat`   | ❌ needs a legacy IDE disk (primary slave, port 0x1F0); modern SATA/AHCI isn't detected |

The OS runs fully (shell, scheduler, memory, demos) in text mode regardless of
graphics or disk.

## Why these fixes exist (robustness notes)

Real firmware is messier than QEMU. NexusOS handles several real-world quirks:

- **LBA disk load.** The bootloader loads the kernel with the int 13h LBA
  extensions (AH=42h), not CHS — CHS multi-track reads hang on many real
  USB-boot BIOSes.
- **E820 fallback.** Some firmware returns a memory map with **no usable
  region** (e.g. an ASUS board under CSM returned only reserved MMIO entries).
  When that happens, the kernel assumes a conservative 128 MiB usable region so
  it can still boot (see `pmm_init` / `PMM_SYNTH_BASE`).
- **Missing-disk handling.** ATA init treats a floating bus (status `0xFF`) as
  "no device" and bounds its wait loop, so a machine with no legacy IDE disk
  doesn't hang the boot.
- **Text-only build.** Some CSM firmware *hangs inside* the VBE `int 0x10` call
  (you can't recover from a hung BIOS call). `make text` builds an image that
  skips VBE entirely and uses the text console.

## Troubleshooting

| Symptom                                   | Likely cause / fix                                                            |
| ----------------------------------------- | ----------------------------------------------------------------------------- |
| Nothing boots / firmware skips the USB    | CSM/Legacy not enabled, or you picked the UEFI boot entry                     |
| Hangs right after the boot banner         | The video BIOS hangs in VBE — use `make text`. (Or a flaky stick — re-flash.) |
| Black screen after the banner             | VBE set a graphics mode but the kernel didn't draw — try `make text`          |
| Boots but keyboard does nothing           | Enable **USB Legacy Support** in firmware                                     |
| `total = 0 MiB` / out of memory           | Old build without the E820 fallback — rebuild from current source             |
| Boots once, hangs on a later re-flash     | Flaky USB write — re-flash, or use a different stick/port                      |
| `[ata] no device on primary channel`      | Expected on SATA/AHCI machines — not an error; `ls`/`cat` just won't work     |

See [ARCHITECTURE.md](ARCHITECTURE.md) for how the boot path and these
subsystems fit together.
