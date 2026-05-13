/* pit.h -- 8253/8254 programmable interval timer on IRQ0. */
#ifndef NEXUS_PIT_H
#define NEXUS_PIT_H

#include "types.h"

void     pit_init(uint32_t hz);
uint64_t pit_ticks(void);

#endif
