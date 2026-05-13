/* kmalloc.c -- first-fit free-list heap.
 *
 * Each block has the layout:
 *
 *      +-------------------+ <- block_header_t
 *      | size              |   includes header bytes
 *      | free flag         |
 *      | prev block ptr    |
 *      | next block ptr    |
 *      +-------------------+ <- payload (returned to caller)
 *      | ... payload ...   |
 *      +-------------------+
 *
 * Blocks form a doubly-linked list in address order. kmalloc walks
 * from the head, first-fit, splits if the block is much larger than
 * requested, and grows the heap (mapping more pages) on miss.
 *
 * Allocations are aligned to 16 bytes, matching the System V ABI's
 * stack alignment guarantee.
 */

#include "kmalloc.h"
#include "vmm.h"
#include "pmm.h"
#include "console.h"
#include "io.h"

#define ALIGN_UP(x, a)   (((x) + ((a) - 1)) & ~((a) - 1))
#define MIN_PAYLOAD      32

typedef struct block_header {
    size_t                size;     /* total bytes incl. this header */
    bool                  free;
    uint8_t               _pad[7];
    struct block_header  *prev;
    struct block_header  *next;
} block_header_t;

#define HDR_SIZE   sizeof(block_header_t)

static block_header_t *heap_head;
static uintptr_t       heap_end_virt;       /* one past last mapped byte */

/* Map `pages` 4 KiB pages at heap_end_virt. Returns true on success. */
static bool grow_heap(size_t pages) {
    for (size_t i = 0; i < pages; i++) {
        uintptr_t pa = pmm_alloc_frame();
        if (!pa) return false;
        if (!vmm_map(heap_end_virt, pa, PTE_KERNEL_RW)) {
            pmm_free_frame(pa);
            return false;
        }
        heap_end_virt += 4096;
    }
    return true;
}

void kmalloc_init(void) {
    heap_end_virt = HEAP_VIRT_BASE;
    if (!grow_heap(HEAP_INITIAL_PAGES)) {
        console_puts("[heap] FATAL: cannot allocate initial heap\n");
        for (;;) { cli(); hlt(); }
    }

    heap_head = (block_header_t *)HEAP_VIRT_BASE;
    heap_head->size = HEAP_INITIAL_PAGES * 4096;
    heap_head->free = true;
    heap_head->prev = NULL;
    heap_head->next = NULL;

    console_puts("[heap] base=");
    console_put_hex(HEAP_VIRT_BASE);
    console_puts(" size=");
    console_put_dec(HEAP_INITIAL_PAGES * 4096);
    console_puts(" B\n");
}

/* Split block `b` so it owns exactly `needed` bytes (including header),
 * returning the trailing remainder as a new free block (or doing nothing
 * if the remainder would be too small to be useful). */
static void try_split(block_header_t *b, size_t needed) {
    if (b->size < needed + HDR_SIZE + MIN_PAYLOAD) return;

    block_header_t *rest = (block_header_t *)((uint8_t *)b + needed);
    rest->size = b->size - needed;
    rest->free = true;
    rest->prev = b;
    rest->next = b->next;
    if (b->next) b->next->prev = rest;
    b->size = needed;
    b->next = rest;
}

void *kmalloc(size_t n) {
    if (n == 0) return NULL;
    size_t needed = ALIGN_UP(n + HDR_SIZE, 16);

    for (block_header_t *b = heap_head; b; b = b->next) {
        if (b->free && b->size >= needed) {
            try_split(b, needed);
            b->free = false;
            return (void *)((uint8_t *)b + HDR_SIZE);
        }
    }

    /* No fit -- grow heap and retry once. */
    size_t pages = (needed + 4095) / 4096;
    if (pages < 16) pages = 16;
    uintptr_t old_end = heap_end_virt;
    if (!grow_heap(pages)) return NULL;

    /* Append new block at old_end and either link it or coalesce
     * with the previous block if that one was free. */
    block_header_t *tail = (block_header_t *)old_end;
    tail->size = pages * 4096;
    tail->free = true;
    tail->next = NULL;

    /* Find current last block. */
    block_header_t *last = heap_head;
    while (last->next) last = last->next;
    tail->prev = last;
    last->next = tail;

    if (last->free) {
        last->size += tail->size;
        last->next  = NULL;
        tail = last;
    }

    if (tail->size < needed) return NULL;
    try_split(tail, needed);
    tail->free = false;
    return (void *)((uint8_t *)tail + HDR_SIZE);
}

void kfree(void *p) {
    if (!p) return;
    block_header_t *b = (block_header_t *)((uint8_t *)p - HDR_SIZE);
    b->free = true;

    /* Coalesce forward. */
    if (b->next && b->next->free) {
        block_header_t *n = b->next;
        b->size += n->size;
        b->next = n->next;
        if (n->next) n->next->prev = b;
    }
    /* Coalesce backward. */
    if (b->prev && b->prev->free) {
        block_header_t *p2 = b->prev;
        p2->size += b->size;
        p2->next = b->next;
        if (b->next) b->next->prev = p2;
    }
}

void kmalloc_stats(uint64_t *used, uint64_t *free) {
    uint64_t u = 0, f = 0;
    for (block_header_t *b = heap_head; b; b = b->next) {
        if (b->free) f += b->size; else u += b->size;
    }
    if (used) *used = u;
    if (free) *free = f;
}
