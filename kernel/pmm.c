/* pmm.c -- bitmap allocator over the E820 memory map.
 *
 * Strategy:
 *   1. Read E820 to find the highest usable physical address.
 *   2. Place the bitmap immediately after the kernel image. The bitmap
 *      needs total_frames bits = total_frames/8 bytes.
 *   3. Mark everything as USED initially.
 *   4. Walk E820 again; for every "usable" region, mark its frames FREE.
 *   5. Reclaim-mark the kernel image, the bitmap itself, and the low
 *      megabyte (BIOS data area, page tables we hand-placed) as USED.
 */

#include "pmm.h"
#include "console.h"
#include "string.h"

#define LOW_MEM_RESERVED  0x100000   /* first 1 MiB: BIOS, bootloader, etc. */

static uint8_t  *bitmap;
static uint64_t  bitmap_bits;        /* number of frames covered */
static uint64_t  bitmap_bytes;
static uint64_t  used_count;
static uint64_t  next_free_hint;     /* small optimisation for alloc */

static inline void  bit_set(uint64_t i)   { bitmap[i >> 3] |=  (uint8_t)(1u << (i & 7)); }
static inline void  bit_clear(uint64_t i) { bitmap[i >> 3] &= (uint8_t)~(1u << (i & 7)); }
static inline bool  bit_test(uint64_t i)  { return (bitmap[i >> 3] >> (i & 7)) & 1; }

static void mark_used_range(uintptr_t base, uint64_t length) {
    uintptr_t end = base + length;
    uint64_t  first = base / PAGE_SIZE;
    uint64_t  last  = (end + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint64_t i = first; i < last && i < bitmap_bits; i++) {
        if (!bit_test(i)) { bit_set(i); used_count++; }
    }
}

static void mark_free_range(uintptr_t base, uint64_t length) {
    /* Round inward so we never free a partially-usable frame. */
    uintptr_t aligned_base = (base + PAGE_SIZE - 1) & ~(uintptr_t)(PAGE_SIZE - 1);
    uintptr_t end          = (base + length) & ~(uintptr_t)(PAGE_SIZE - 1);
    if (end <= aligned_base) return;

    uint64_t first = aligned_base / PAGE_SIZE;
    uint64_t last  = end / PAGE_SIZE;
    for (uint64_t i = first; i < last && i < bitmap_bits; i++) {
        if (bit_test(i)) { bit_clear(i); used_count--; }
    }
}

void pmm_init(uintptr_t kernel_end_phys) {
    uint32_t        count   = *(volatile uint32_t *)E820_COUNT_ADDR;
    e820_entry_t   *entries = (e820_entry_t *)E820_ENTRIES_ADDR;

    /* Find highest physical address claimed by USABLE E820 entries.
     * Including reserved/MMIO regions (PCI hole at ~1 TiB on QEMU) would
     * balloon the bitmap into tens of MiB and overrun the early 2 MiB
     * identity map, clobbering the page tables at 0x70000. */
    uint64_t max_addr = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (entries[i].type != 1) continue;
        uint64_t top = entries[i].base + entries[i].length;
        if (top > max_addr) max_addr = top;
    }
    bitmap_bits  = (max_addr + PAGE_SIZE - 1) / PAGE_SIZE;
    bitmap_bytes = (bitmap_bits + 7) / 8;

    /* Place bitmap after kernel, page-aligned. */
    uintptr_t bitmap_pa = (kernel_end_phys + PAGE_SIZE - 1) & ~(uintptr_t)(PAGE_SIZE - 1);
    bitmap = (uint8_t *)bitmap_pa;

    /* Start with everything marked used. */
    memset(bitmap, 0xFF, bitmap_bytes);
    used_count = bitmap_bits;

    /* Free what E820 says is usable. */
    for (uint32_t i = 0; i < count; i++) {
        if (entries[i].type == 1) {
            mark_free_range((uintptr_t)entries[i].base, entries[i].length);
        }
    }

    /* Reserve low memory, kernel image, and the bitmap itself. */
    mark_used_range(0, LOW_MEM_RESERVED);
    mark_used_range(LOW_MEM_RESERVED,
                    (kernel_end_phys > LOW_MEM_RESERVED)
                        ? (kernel_end_phys - LOW_MEM_RESERVED) : 0);
    mark_used_range(bitmap_pa, bitmap_bytes);

    next_free_hint = LOW_MEM_RESERVED / PAGE_SIZE;

    console_puts("[pmm] total = ");
    console_put_dec(bitmap_bits * PAGE_SIZE / (1024 * 1024));
    console_puts(" MiB, free = ");
    console_put_dec((bitmap_bits - used_count) * PAGE_SIZE / (1024 * 1024));
    console_puts(" MiB\n");
}

uintptr_t pmm_alloc_frame(void) {
    for (uint64_t pass = 0; pass < 2; pass++) {
        uint64_t start = (pass == 0) ? next_free_hint : 0;
        uint64_t stop  = (pass == 0) ? bitmap_bits   : next_free_hint;
        for (uint64_t i = start; i < stop; i++) {
            if (!bit_test(i)) {
                bit_set(i);
                used_count++;
                next_free_hint = i + 1;
                return (uintptr_t)i * PAGE_SIZE;
            }
        }
    }
    return 0;
}

void pmm_free_frame(uintptr_t pa) {
    uint64_t i = pa / PAGE_SIZE;
    if (i >= bitmap_bits) return;
    if (bit_test(i)) {
        bit_clear(i);
        used_count--;
        if (i < next_free_hint) next_free_hint = i;
    }
}

uint64_t pmm_total_frames(void) { return bitmap_bits; }
uint64_t pmm_used_frames(void)  { return used_count; }
