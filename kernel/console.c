/* console.c -- writes go to both COM1 (serial) and the VGA text buffer. */
#include "console.h"
#include "io.h"
#include "string.h"

#define COM1            0x3F8
#define VGA_BUFFER      ((volatile uint16_t *)0xB8000)
#define VGA_W           80
#define VGA_H           25
#define VGA_ATTR        0x0F            /* white on black */

static uint32_t vga_cursor = 0;

static void serial_init(void) {
    outb(COM1 + 1, 0x00);   /* disable interrupts */
    outb(COM1 + 3, 0x80);   /* DLAB on */
    outb(COM1 + 0, 0x01);   /* divisor low  -> 115200 baud */
    outb(COM1 + 1, 0x00);   /* divisor high */
    outb(COM1 + 3, 0x03);   /* 8N1, DLAB off */
    outb(COM1 + 2, 0xC7);   /* FIFO on, clear, 14-byte threshold */
    outb(COM1 + 4, 0x0B);   /* IRQs enabled, RTS/DSR set */
}

static void serial_putc(char c) {
    while (!(inb(COM1 + 5) & 0x20)) { }
    outb(COM1, (uint8_t)c);
}

static void vga_scroll(void) {
    /* move rows 1..H-1 up by one */
    for (uint32_t row = 1; row < VGA_H; row++) {
        for (uint32_t col = 0; col < VGA_W; col++) {
            VGA_BUFFER[(row - 1) * VGA_W + col] =
                VGA_BUFFER[row * VGA_W + col];
        }
    }
    /* clear last row */
    for (uint32_t col = 0; col < VGA_W; col++) {
        VGA_BUFFER[(VGA_H - 1) * VGA_W + col] =
            (uint16_t)' ' | (VGA_ATTR << 8);
    }
    vga_cursor -= VGA_W;
}

static void vga_putc(char c) {
    if (c == '\n') {
        vga_cursor += VGA_W - (vga_cursor % VGA_W);
    } else if (c == '\r') {
        vga_cursor -= (vga_cursor % VGA_W);
    } else {
        VGA_BUFFER[vga_cursor++] = (uint16_t)(uint8_t)c | (VGA_ATTR << 8);
    }
    if (vga_cursor >= VGA_W * VGA_H) vga_scroll();
}

void console_init(void) {
    serial_init();
    /* clear screen */
    for (uint32_t i = 0; i < VGA_W * VGA_H; i++)
        VGA_BUFFER[i] = (uint16_t)' ' | (VGA_ATTR << 8);
    vga_cursor = 0;
}

void console_putc(char c) {
    if (c == '\n') serial_putc('\r');
    serial_putc(c);
    vga_putc(c);
}

void console_puts(const char *s) {
    while (*s) console_putc(*s++);
}

void console_put_hex(uint64_t v) {
    char buf[17];
    u64_to_hex(v, buf);
    console_puts("0x");
    console_puts(buf);
}

void console_put_dec(uint64_t v) {
    char buf[21];
    u64_to_dec(v, buf);
    console_puts(buf);
}
