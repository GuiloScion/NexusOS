/* string.h -- freestanding mem/string helpers. */
#ifndef NEXUS_STRING_H
#define NEXUS_STRING_H

#include "types.h"

void   *memset(void *dst, int val, size_t n);
void   *memcpy(void *dst, const void *src, size_t n);
int     memcmp(const void *a, const void *b, size_t n);
size_t  strlen(const char *s);

/* Convert uint64 to hex string (16 chars, no prefix). buf must hold >=17 bytes. */
void    u64_to_hex(uint64_t v, char *buf);

/* Convert uint64 to decimal. buf must hold >=21 bytes. */
void    u64_to_dec(uint64_t v, char *buf);

#endif
