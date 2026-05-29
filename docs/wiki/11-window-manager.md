# 11. A window manager: the mouse, a compositor, and windows

[← Graphics](10-framebuffer-graphics.md) · [Home](README.md) · [Next: Real hardware →](12-real-hardware.md)

This is the chapter where it stops looking like a kernel and starts looking like
a *computer*. We'll add a mouse, then build a **compositor** that draws
overlapping windows with a cursor on top — the heart of any graphical desktop.

## The mouse (PS/2, IRQ12)

The PS/2 mouse is the keyboard's sibling. It lives on the 8042 controller's
*second* port, fires **IRQ12**, and sends 3-byte packets:

- **byte 0:** button states + sign bits + a marker bit that's always 1.
- **byte 1:** X movement (signed).
- **byte 2:** Y movement (signed — and positive means *up*, so you subtract it
  from your screen Y).

Setup (`kernel/mouse.c`): enable the aux port and IRQ12 in the controller config,
tell the mouse to use defaults and start reporting, then in the IRQ handler
assemble the 3 bytes into packets and update an `(x, y)` position clamped to the
screen. Movements are *relative* deltas; you accumulate them.

> **Pitfall:** the marker bit in byte 0 lets you re-sync if you ever read a byte
> mid-packet. If your cursor jumps wildly, you've lost packet alignment — check
> that bit and drop the byte if it's wrong.

## The naive approach (and why it fails)

The obvious way to draw a cursor: save the pixels under it, draw the arrow, and
when it moves, restore the saved pixels and repeat. This *works* until something
else draws on the screen underneath the cursor — then your "restore" paints stale
pixels back over the new content. You get trails and corruption. NexusOS shipped
this in an early version and immediately felt the pain.

## The fix: a compositor with a back buffer

The professional approach is **double buffering + compositing**:

1. Keep an **off-screen back buffer** the same size as the screen (allocate it
   from your heap — for 1024×768 that's ~2.4 MB).
2. To draw a frame, **compose** the whole scene into the back buffer from
   scratch: desktop background, then each window in z-order.
3. **Present**: blit the back buffer to the real framebuffer in one shot, then
   draw the mouse cursor on top.

Because you rebuild the scene every frame from a *model* (window list, console
grid), nothing is ever stale. And because the cursor is painted *after* the blit
and never stored in the scene, **it can never clobber what's underneath** — the
trails problem simply disappears. This is `kernel/wm.c`.

NexusOS runs the compositor as its own [scheduler](08-multitasking.md) task: it
recomposes when something changed (a "dirty" flag, plus a version counter on the
console grid) and presents every frame so the cursor stays responsive.

## Windows as data

A window is just a struct: position, size, title, z-order, a few flags
(minimized, closable), and its contents. The window manager keeps a list and a
**z-order**. Composing is then: for each window from back to front, draw its
border, title bar (highlighted if focused), and body.

Interaction falls out of hit-testing the mouse against that list:

- **Click** → find the top-most window under the cursor, raise it to the front,
  give it focus.
- **Drag the title bar** → move that window with the mouse.
- **Close / minimize buttons** → small rectangles in the title bar you hit-test.
- **Taskbar** → a strip with a button per window (and a clock); clicking restores
  / raises.

None of this needs new graphics primitives — it's all rectangles and text on a
surface, plus a little geometry.

## Putting the shell in a window

Remember the console is a **character grid** ([Chapter 10](10-framebuffer-graphics.md))?
The compositor can render that grid *inside a window's body* instead of full
screen. Now the shell lives in a draggable "Terminal" window, and the rest of the
screen is a desktop with other windows. That's NexusOS's default desktop.

## Try it

- Add a fourth window with your own text. Drag it over the others — confirm no
  artifacts (that's your compositor working).
- Drive it headlessly: QEMU's monitor can inject mouse motion and clicks via
  `input-send-event`, so you can script "click here, drag there" and screenshot
  the result. NexusOS used exactly this to test the WM without a human.
- Give windows a resize handle, or add a second terminal.

## Next

You have a graphical, interactive OS — in an emulator. The real test is booting
it on a physical machine, where the firmware fights back. That's where the most
valuable lessons live.

[← Graphics](10-framebuffer-graphics.md) · [Home](README.md) · [Next: Real hardware →](12-real-hardware.md)
