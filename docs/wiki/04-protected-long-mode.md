# 4. From 16-bit to 64-bit: protected mode, paging, and long mode

[← How a PC boots](03-how-a-pc-boots.md) · [Home](README.md) · [Next: First C kernel →](05-first-c-kernel.md)

The CPU boots in 16-bit real mode for compatibility, but we want the full 64-bit
machine: all of RAM, memory protection, and modern instructions. Getting there
is a fixed three-step ritual. It looks intimidating, but it's just a checklist.

## The three modes

- **Real mode (16-bit):** where we start. 1 MiB addressable, no protection.
- **Protected mode (32-bit):** 4 GiB addressable, segmentation + protection,
  enables paging. A stepping stone.
- **Long mode (64-bit):** the goal. Requires paging to be on. This is where the
  kernel runs.

You go real → protected → long. You can't skip to long mode directly.

## Step 1: A20 and the GDT

**A20 line.** For historical reasons, the 21st address line is disabled at boot
(memory wraps at 1 MiB). Enable it so you can use memory above 1 MiB. The quick
way (the "fast A20" gate):

```asm
    in  al, 0x92
    or  al, 2
    out 0x92, al
```

**GDT (Global Descriptor Table).** Protected mode replaces real mode's
segment math with *descriptors* that define memory regions and their
permissions. For a flat memory model you just need a few entries: a null
descriptor, a code segment, a data segment, and (for long mode) a 64-bit code
segment. NexusOS's GDT in `bootloader/boot.asm`:

```asm
gdt_start:
    dq 0x0000000000000000   ; null (required)
    dq 0x00CF9A000000FFFF   ; 32-bit code
    dq 0x00CF92000000FFFF   ; 32-bit data
    dq 0x00AF9A000000FFFF   ; 64-bit code
gdt_end:
```

Those magic numbers are just permission/limit bits packed into a 64-bit
descriptor; you can copy a known-good GDT and move on. Load it with `lgdt`.

## Step 2: enter protected mode

Set bit 0 of control register `cr0`, then far-jump to flush the pipeline and
load the new code segment:

```asm
    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or  eax, 1          ; CR0.PE = protected mode enable
    mov cr0, eax
    jmp 0x08:protected_mode   ; 0x08 = our 32-bit code selector
```

You're now in 32-bit protected mode.

## Step 3: paging, then long mode

64-bit long mode **requires paging** to be enabled. Paging is the mechanism that
maps *virtual* addresses (what code uses) to *physical* addresses (actual RAM) —
the foundation of memory management (Chapter 7). To get into long mode you only
need a *minimal* set of page tables: enough to map the kernel so it can run.

x86-64 paging is a 4-level tree: **PML4 → PDPT → PD → PT**, each a table of 512
entries. A simple bootstrap maps the first 2 MiB with a single "huge" 2 MiB
page. NexusOS does this in `kernel/kernel_entry.asm`:

```asm
    ; build tiny page tables at 0x70000
    mov dword [0x70000], 0x71000 | 3   ; PML4[0] -> PDPT
    mov dword [0x71000], 0x72000 | 3   ; PDPT[0] -> PD
    mov dword [0x72000], 0x00000000 | 0x83  ; PD[0] -> 2 MiB page at 0 (PS bit)
    mov eax, 0x70000
    mov cr3, eax                ; CR3 points the CPU at the page tables

    mov eax, cr4
    or  eax, 1 << 5             ; CR4.PAE (physical address extension)
    mov cr4, eax

    mov ecx, 0xC0000080         ; the EFER model-specific register
    rdmsr
    or  eax, 1 << 8             ; EFER.LME = long mode enable
    wrmsr

    mov eax, cr0
    or  eax, 1 << 31            ; CR0.PG = paging on  -> now in long mode
    mov cr0, eax

    jmp 0x18:long_mode          ; 0x18 = our 64-bit code selector
```

After that far jump you're executing 64-bit code. The `| 3` and `| 0x83` are
page-table flags (present, writable, and "this is a 2 MiB page").

## Why two stages of page tables?

This bootstrap only maps the first 2 MiB — just enough to run the kernel's entry
code. Once the kernel is up and has a *physical memory allocator*, it builds
real page tables covering all of RAM (Chapter 7). It's a classic pattern:
hand-place a minimal mapping to get going, then replace it properly later.

## Landing in the kernel

The 64-bit entry stub finishes the handoff:

```asm
long_mode:
    mov ax, 0x10        ; data selector into the segment registers
    mov ds, ax
    mov ss, ax
    mov rsp, 0x90000    ; set up a stack
    ; zero the .bss section (uninitialized globals must start at 0)
    ...
    call kernel_main    ; into C!
```

Zeroing `.bss` matters: C assumes uninitialized globals are zero, but the flat
binary doesn't store them, so the kernel must clear that region itself.

## The big picture

```
real mode ──A20+GDT──> protected mode ──page tables+PAE+LME+PG──> long mode ──> kernel_main()
```

It's a lot of one-time ceremony, but it's *fixed* — you set it up once and never
think about it again. From `kernel_main()` onward, you're writing normal C.

[← How a PC boots](03-how-a-pc-boots.md) · [Home](README.md) · [Next: First C kernel →](05-first-c-kernel.md)
