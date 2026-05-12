/* NexusOS kernel
 *
 * Entered from kernel_entry.asm in 64-bit long mode with the low 2 MiB
 * identity-mapped (so the VGA buffer at 0xB8000 is accessible) and a
 * stack at 0x90000.
 */

#define VGA_ADDRESS 0xB8000
#define VGA_WIDTH   80
#define VGA_HEIGHT  25
#define COM1_PORT   0x3F8

typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef unsigned long  uint64_t;

static volatile uint16_t *vga_buffer = (volatile uint16_t *)VGA_ADDRESS;
static uint32_t vga_offset = 0;

/* ---------- Port I/O helpers ---------- */

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* ---------- Serial (COM1) ---------- */

void serial_init(void) {
    outb(COM1_PORT + 1, 0x00);  /* disable interrupts */
    outb(COM1_PORT + 3, 0x80);  /* enable DLAB */
    outb(COM1_PORT + 0, 0x01);  /* divisor low  (115200 baud) */
    outb(COM1_PORT + 1, 0x00);  /* divisor high */
    outb(COM1_PORT + 3, 0x03);  /* 8N1, DLAB off */
    outb(COM1_PORT + 2, 0xC7);  /* enable FIFO, clear, 14-byte threshold */
    outb(COM1_PORT + 4, 0x0B);  /* IRQs enabled, RTS/DSR set */
}

static void serial_putchar(char c) {
    while (!(inb(COM1_PORT + 5) & 0x20)) { /* wait for THR empty */ }
    outb(COM1_PORT, (uint8_t)c);
}

void serial_print(const char *str) {
    while (*str) {
        if (*str == '\n') serial_putchar('\r');
        serial_putchar(*str++);
    }
}

/* ---------- VGA text mode ---------- */

void kernel_putchar(char c) {
    if (c == '\n') {
        vga_offset += VGA_WIDTH - (vga_offset % VGA_WIDTH);
        return;
    }
    vga_buffer[vga_offset] = (uint16_t)c | (0x0F << 8);
    vga_offset++;
}

void kernel_print(const char *str) {
    while (*str) kernel_putchar(*str++);
}

/* ---------- Entry point ---------- */

void kernel_main(void) {
    serial_init();

    serial_print("=================================\n");
    serial_print("      Welcome to NexusOS\n");
    serial_print("=================================\n");
    serial_print("Kernel loaded successfully.\n");
    serial_print("System initialized.\n");
    serial_print("Running on x86-64 bare metal.\n");
    serial_print("=================================\n");

    kernel_print("NexusOS Online");

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
