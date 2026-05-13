/* vmm.h -- 4-level page tables, 4 KiB granularity.
 *
 * After vmm_init():
 *   - Physical [0 .. RAM_END] is identity-mapped using 2 MiB pages.
 *   - Higher virtual ranges can be mapped on demand with vmm_map() using
 *     4 KiB pages, with page tables themselves backed by frames from PMM.
 */
#ifndef NEXUS_VMM_H
#define NEXUS_VMM_H

#include "types.h"

/* Page-table entry flag bits. */
#define PTE_PRESENT     (1ull << 0)
#define PTE_WRITE       (1ull << 1)
#define PTE_USER        (1ull << 2)
#define PTE_PWT         (1ull << 3)
#define PTE_PCD         (1ull << 4)
#define PTE_ACCESSED    (1ull << 5)
#define PTE_DIRTY       (1ull << 6)
#define PTE_HUGE        (1ull << 7)
#define PTE_GLOBAL      (1ull << 8)

#define PTE_KERNEL_RW   (PTE_PRESENT | PTE_WRITE)
#define PTE_KERNEL_RO   (PTE_PRESENT)

void      vmm_init(uint64_t ram_end);
bool      vmm_map(uintptr_t virt, uintptr_t phys, uint64_t flags);
void      vmm_unmap(uintptr_t virt);
uintptr_t vmm_translate(uintptr_t virt);     /* 0 if unmapped */

#endif
