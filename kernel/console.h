/* console.h -- unified print to serial + VGA text mode. */
#ifndef NEXUS_CONSOLE_H
#define NEXUS_CONSOLE_H

#include "types.h"

void console_init(void);
void console_putc(char c);
void console_puts(const char *s);
void console_put_hex(uint64_t v);   /* prints "0x" + 16 hex digits */
void console_put_dec(uint64_t v);

#endif
