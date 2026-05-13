/* string.c */
#include "string.h"

void *memset(void *dst, int val, size_t n) {
    uint8_t *p = (uint8_t *)dst;
    uint8_t  v = (uint8_t)val;
    while (n--) *p++ = v;
    return dst;
}

void *memcpy(void *dst, const void *src, size_t n) {
    uint8_t       *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) *d++ = *s++;
    return dst;
}

int memcmp(const void *a, const void *b, size_t n) {
    const uint8_t *pa = a;
    const uint8_t *pb = b;
    while (n--) {
        if (*pa != *pb) return (int)*pa - (int)*pb;
        pa++; pb++;
    }
    return 0;
}

size_t strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

void u64_to_hex(uint64_t v, char *buf) {
    static const char digits[] = "0123456789ABCDEF";
    for (int i = 15; i >= 0; i--) {
        buf[i] = digits[v & 0xF];
        v >>= 4;
    }
    buf[16] = '\0';
}

void u64_to_dec(uint64_t v, char *buf) {
    char tmp[21];
    int  n = 0;
    if (v == 0) { tmp[n++] = '0'; }
    while (v) { tmp[n++] = '0' + (v % 10); v /= 10; }
    /* reverse */
    int j = 0;
    while (n--) buf[j++] = tmp[n];
    buf[j] = '\0';
}
