#define VGA_ADDRESS 0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define COM1_PORT 0x3F8

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long uint64_t;

static uint16_t *vga_buffer = (uint16_t *)VGA_ADDRESS;
static uint32_t vga_offset = 0;

void serial_init(void) {
    // Initialize COM1 serial port
    uint8_t ier = 0;
    uint8_t lcr = 0x80;  // Enable DLAB
    uint8_t dll = 1;     // Divisor low byte (115200 baud)
    uint8_t dlm = 0;     // Divisor high byte
    
    __asm__ volatile("outb %0, %1" : : "a"(lcr), "Nd"(COM1_PORT + 3));
    __asm__ volatile("outb %0, %1" : : "a"(dll), "Nd"(COM1_PORT + 0));
    __asm__ volatile("outb %0, %1" : : "a"(dlm), "Nd"(COM1_PORT + 1));
    
    lcr = 0x03;  // 8 bits, 1 stop bit, no parity
    __asm__ volatile("outb %0, %1" : : "a"(lcr), "Nd"(COM1_PORT + 3));
    
    ier = 0;
    __asm__ volatile("outb %0, %1" : : "a"(ier), "Nd"(COM1_PORT + 1));
}

void serial_putchar(char c) {
    // Wait for transmit hold register to be empty
    uint8_t status;
    do {
        __asm__ volatile("inb %1, %0" : "=a"(status) : "Nd"(COM1_PORT + 5));
    } while (!(status & 0x20));
    
    // Send character
    __asm__ volatile("outb %0, %1" : : "a"((uint8_t)c), "Nd"(COM1_PORT));
}

void serial_print(const char *str) {
    while (*str) {
        if (*str == '\n') {
            serial_putchar('\r');
        }
        serial_putchar(*str++);
    }
}

void kernel_putchar(char c) {
    if (c == '\n') {
        vga_offset += VGA_WIDTH - (vga_offset % VGA_WIDTH);
        return;
    }
    
    vga_buffer[vga_offset] = (uint16_t)c | (0x0F << 8);
    vga_offset++;
}

void kernel_print(const char *str) {
    while (*str) {
        kernel_putchar(*str++);
    }
}

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
    
    // Halt
    while (1) {
        __asm__("hlt");
    }
}
