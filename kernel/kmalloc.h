/* kmalloc.h -- kernel heap.
 *
 * A first-fit free-list allocator. Backing pages are demand-mapped via
 * the VMM from frames the PMM hands out. The heap lives in a virtual
 * address range chosen to sit above the identity-mapped region.
 */
#ifndef NEXUS_KMALLOC_H
#define NEXUS_KMALLOC_H

#include "types.h"

#define HEAP_VIRT_BASE      0x0000000200000000ull   /* 8 GiB */
#define HEAP_INITIAL_PAGES  16                       /* 64 KiB initial size */

void   kmalloc_init(void);
void  *kmalloc(size_t n);
void   kfree(void *p);

/* Diagnostic */
void   kmalloc_stats(uint64_t *used, uint64_t *free);

#endif
