/* types.h -- minimal fixed-width integer types for the kernel. */
#ifndef NEXUS_TYPES_H
#define NEXUS_TYPES_H

typedef unsigned char       uint8_t;
typedef unsigned short      uint16_t;
typedef unsigned int        uint32_t;
typedef unsigned long       uint64_t;

typedef signed char         int8_t;
typedef signed short        int16_t;
typedef signed int          int32_t;
typedef signed long         int64_t;

typedef uint64_t            size_t;
typedef int64_t             ssize_t;
typedef uint64_t            uintptr_t;
typedef int64_t             intptr_t;

typedef _Bool               bool;
#define true                1
#define false               0

#define NULL                ((void *)0)

#define PACKED              __attribute__((packed))
#define ALIGN(n)            __attribute__((aligned(n)))
#define NORETURN            __attribute__((noreturn))
#define UNUSED              __attribute__((unused))

#endif
