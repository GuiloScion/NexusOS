# 10. Graphics: the framebuffer, fonts, and a console

[← Storage](09-storage.md) · [Home](README.md) · [Next: Window manager →](11-window-manager.md)

So far everything has been text. Now we go graphical — and it turns out drawing
a pixel is *easier* than you'd expect. The hard part is getting the hardware
into a mode where a pixel is just a memory write.

## The idea: a linear framebuffer

In graphics mode, the screen is a big array in memory called the **framebuffer**.
Each pixel is a few bytes (3 for 24-bit color, 4 for 32-bit). To light up a
pixel you just write its color to the right address. No drawing commands, no GPU
API — just memory.

The screen is laid out row by row. The address of pixel `(x, y)` is:

```
addr = framebuffer_base + y * pitch + x * (bytes_per_pixel)
```

Two things trip everyone up:

- **Pitch, not width.** `pitch` (a.k.a. stride) is the number of *bytes* per row.
  It is often **larger** than `width * bytes_per_pixel` because rows can be padded
  for alignment. Always use the pitch the firmware reports — never assume
  `width * bpp`.
- **Byte order.** 24-bit color is usually stored **B, G, R** in memory, not R, G,
  B. Get it backwards and your reds and blues swap.

## Getting into a graphics mode (VESA/VBE)

A modern card can do graphics, but *asking* it to is a BIOS service called
**VBE** (VESA BIOS Extensions), and BIOS services only work in **real mode** —
before we jump to the kernel. So the *bootloader* sets the mode.

In NexusOS, `bootloader/boot.asm` (still in 16-bit real mode) calls:

- `int 0x10, AX=0x4F01` — *get mode info* for a 1024×768 mode, which returns the
  framebuffer's physical address, pitch, dimensions, and bits-per-pixel.
- `int 0x10, AX=0x4F02` — *set* that mode, with the "linear framebuffer" bit.

It then leaves a little descriptor (address, pitch, width, height, bpp, and a
"valid" flag) at a known memory address (`0x9700`) for the kernel to read. If
VBE fails, it sets valid = 0 and the kernel stays on the text console.

> **Pitfall:** the moment you switch to a graphics mode, the old VGA *text*
> buffer at `0xB8000` stops showing anything. So switching modes and having
> something to draw must come together — otherwise you get a black screen and
> think you've crashed. (Keep the serial port as your debug lifeline.)

## Mapping the framebuffer

The framebuffer's physical address is typically *high* (NexusOS sees
`0xFD000000` in QEMU) — above the RAM your paging identity-mapped in
[Chapter 8](08-multitasking.md). It is memory-mapped I/O, not RAM, so the kernel
must map those pages explicitly before touching them. `kernel/fb.c` reads the
descriptor and `vmm_map`s the framebuffer range page by page, then exposes raw
pixel access.

## Drawing: surfaces and primitives

You don't want every drawing routine hard-coded to "the screen." NexusOS uses a
**surface** abstraction (`kernel/gfx.c`): a `surface_t` is *any* pixel buffer —
the real screen, or an off-screen buffer — described by its address, pitch,
size, and bpp. Every primitive (`gfx_pixel`, `gfx_fill`, `gfx_blit`,
`gfx_glyph`, `gfx_text`) takes a surface, so the same code draws to the screen or
to a back buffer. (That pays off hugely in the [next chapter](11-window-manager.md).)

## Text without the BIOS: a bitmap font

In graphics mode there's no "print character" anymore — you draw letters
yourself. A **bitmap font** is just an array: for each character, 8 bytes, where
each byte is a row and each bit is a pixel (1 = draw, 0 = skip). NexusOS embeds a
public-domain 8×8 font in `kernel/font8x8.h`; `gfx_glyph` reads those bits and
fills a (scaled) block per set bit.

## A console on the framebuffer

To replace the text console, NexusOS keeps a **character grid** in
`kernel/fbcon.c` — a 2D array of characters plus a cursor. `console_*` writes
update the grid (handling newline, backspace, scroll); a separate render step
draws the grid using the font. Storing the console as *data* (not as pixels you
drew once) is the key trick — it means the screen can be **re-rendered at any
time** from the grid, which is exactly what a compositor needs.

## Try it

- Clear the screen to a color, then draw three colored rectangles. (NexusOS's
  first graphics commit did exactly this as a smoke test.)
- Draw a string with your font at a few scales.
- Capture what you drew without a monitor: QEMU's monitor `screendump` writes the
  framebuffer to an image file. NexusOS automates this in `tools/screenshot.sh` —
  invaluable for headless/CI testing.

## Next

You can draw pixels, text, and rectangles to an off-screen surface and blit it to
the screen. That's everything you need to build a **windowing GUI** — windows, a
mouse cursor, and a compositor.

[← Storage](09-storage.md) · [Home](README.md) · [Next: Window manager →](11-window-manager.md)
