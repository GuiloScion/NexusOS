/* pmm.h -- Physical Memory Manager.
 *
 * Owns the set of 4 KiB physical frames. A bitmap allocator: bit i = 1
 * means frame i (physical address i*4096) is in use.
 */
#ifndef NEXUS_PMM_H
#define NEXUS_PMM_H

#include "types.h"

#define PAGE_SIZE       4096
#define PAGE_SHIFT      12

/* Fallback usable region when firmware's E820 reports no usable RAM. */
#define PMM_SYNTH_BASE  0x100000     /* 1 MiB   */
#define PMM_SYNTH_LEN   0x8000000    /* 128 MiB */

/* E820 entry as the BIOS hands it back. */
typedef struct PACKED {
    uint64_t base;
    uint64_t length;
    uint32_t type;          /* 1 = usable */
    uint32_t acpi_attrs;
} e820_entry_t;

/* The bootloader stores the count at 0x9000 and the entries at 0x9008. */
#define E820_COUNT_ADDR   0x9000
#define E820_ENTRIES_ADDR 0x9008

void      pmm_init(uintptr_t kernel_end_phys);
uintptr_t pmm_alloc_frame(void);          /* returns physical addr, 0 on OOM */
void      pmm_free_frame(uintptr_t pa);
uint64_t  pmm_total_frames(void);
uint64_t  pmm_used_frames(void);

#endif
