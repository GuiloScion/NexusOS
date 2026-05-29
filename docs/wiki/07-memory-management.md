# 7. Memory management: physical frames, paging, and the heap

[← Interrupts](06-interrupts.md) · [Home](README.md) · [Next: Multitasking →](08-multitasking.md)

Almost everything the kernel does eventually needs memory: page tables, task
stacks, buffers, a heap. Managing memory is three layered problems, each built on
the one below:

1. **Physical memory** — which 4 KiB chunks of real RAM are free? (the PMM)
2. **Virtual memory** — map virtual addresses to physical ones via page tables.
   (the VMM)
3. **The heap** — `kmalloc`/`kfree` for arbitrary-sized allocations.

## Layer 1: the physical memory manager (PMM)

First, *how much RAM is there, and where?* You can't probe this safely by poking
addresses. Instead the BIOS told you, via the **E820 map** the bootloader saved
in real mode. Each entry is `(base, length, type)`; `type == 1` means usable
RAM, everything else is reserved (firmware, MMIO, ACPI).

A simple and robust PMM is a **bitmap allocator**: one bit per 4 KiB physical
*frame*, `1` = used, `0` = free. NexusOS's `kernel/pmm.c` does this:

1. Scan E820 for the highest usable address → size the bitmap.
2. Place the bitmap just past the kernel image (`__kernel_end`).
3. Mark **everything used**, then **free** each usable E820 region.
4. Re-reserve the low 1 MiB, the kernel image, and the bitmap itself.

```c
uintptr_t pmm_alloc_frame(void);   // returns a free physical address, 0 if none
void      pmm_free_frame(uintptr_t pa);
```

> **Real-hardware lesson:** some firmware returns an E820 map with *no usable
> region at all*. Rather than fail, NexusOS falls back to assuming a conservative
> 128 MiB is usable (`PMM_SYNTH_BASE`/`PMM_SYNTH_LEN`). Defensive fallbacks like
> this are what make an OS survive the messiness of real machines — see
> [HARDWARE.md](../HARDWARE.md).

## Layer 2: virtual memory (paging)

Paging maps **virtual addresses** (what code uses) to **physical frames** (real
RAM), in 4 KiB pages, through the 4-level table tree (PML4→PDPT→PD→PT) the boot
stub set up minimally. Now that you have a PMM, you build *real* page tables.

The VMM's core operation is `map`: given a virtual address, a physical address,
and flags, walk the four levels — allocating a new table frame from the PMM
wherever one is missing — and write the final entry:

```c
bool vmm_map(uintptr_t virt, uintptr_t phys, uint64_t flags);
void vmm_unmap(uintptr_t virt);
uintptr_t vmm_translate(uintptr_t virt);   // reverse lookup
```

NexusOS's `kernel/vmm.c` **identity-maps** all of RAM (virtual == physical) with
2 MiB pages for simplicity, installs a fresh kernel PML4 (`mov cr3`), and exposes
the 4 KiB API above for fine-grained mappings (like the heap and the
framebuffer). After remapping you must reload `CR3` (or `invlpg` a single page)
so the CPU's TLB cache picks up the change.

Why identity-map? It's the simplest choice: pointers are physical addresses, no
translation to reason about. Later, real process isolation uses *separate*
address spaces per process — but that's a Chapter 13 concern.

## Layer 3: the kernel heap (`kmalloc`)

Frames are 4 KiB; you often want 37 bytes, or 9000. A **heap** sits on top of the
VMM and hands out arbitrary sizes. NexusOS's `kernel/kmalloc.c` is a classic
**first-fit free list**:

- The heap lives at a high virtual address (`0x200000000`).
- It's **demand-grown**: when it needs more space, it asks the PMM for frames and
  `vmm_map`s them in — so unused heap costs nothing.
- Each block has a header (size + free flag) in a doubly-linked list; `kmalloc`
  finds the first block big enough (splitting it), `kfree` marks it free and
  **coalesces** adjacent free blocks to fight fragmentation.

```c
void *kmalloc(size_t n);
void  kfree(void *p);
```

Once `kmalloc` works, the rest of the kernel gets much easier — drivers and the
window manager just allocate what they need.

## The stack of abstractions

```
kmalloc / kfree          (arbitrary sizes)
   └── vmm_map           (virtual → physical, 4 KiB pages)
         └── pmm_alloc_frame   (free physical frames)
               └── E820 map    (what RAM exists)
```

Each layer only knows about the one below it. That separation is what keeps a
memory manager understandable — and it's a pattern you'll reuse everywhere in
the kernel.

## Try it

- Print `pmm_total_frames()` and `pmm_used_frames()` at boot — watch free memory.
- `kmalloc` a few blocks, `kfree` them, and assert the heap's free count returns
  to where it started. (NexusOS's `mem` shell command shows live heap stats.)

With memory under control, we can finally make the kernel do *several things at
once*.

[← Interrupts](06-interrupts.md) · [Home](README.md) · [Next: Multitasking →](08-multitasking.md)
