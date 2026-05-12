#define SERIAL_PORT 0x3F8
#define VGA_ADDRESS 0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long uint64_t;

static uint16_t *vga_buffer = (uint16_t *)VGA_ADDRESS;
static uint32_t vga_offset = 0;

/* Serial port functions */
void serial_init(void) {
    // Initialize serial port (COM1 at 0x3F8)
    // Set baud rate to 115200
    __asm__ volatile("outb %0, %1" : : "a"(0x80), "Nd"(SERIAL_PORT + 3)); // DLAB
    __asm__ volatile("outb %0, %1" : : "a"(0x01), "Nd"(SERIAL_PORT + 0)); // Low byte
    __asm__ volatile("outb %0, %1" : : "a"(0x00), "Nd"(SERIAL_PORT + 1)); // High byte
    __asm__ volatile("outb %0, %1" : : "a"(0x03), "Nd"(SERIAL_PORT + 3)); // 8 bits, 1 stop
    __asm__ volatile("outb %0, %1" : : "a"(0x00), "Nd"(SERIAL_PORT + 1)); // No interrupts
}

int serial_transmit_empty(void) {
    uint8_t status;
    __asm__ volatile("inb %1, %0" : "=a"(status) : "Nd"(SERIAL_PORT + 5));
    return status & 0x20;
}

void serial_putchar(char c) {
    while (!serial_transmit_empty());
    __asm__ volatile("outb %0, %1" : : "a"((uint8_t)c), "Nd"(SERIAL_PORT));
}

void serial_print(const char *str) {
    while (*str) {
        if (*str == '\n') {
            serial_putchar('\r');
            serial_putchar('\n');
        } else {
            serial_putchar(*str);
        }
        str++;
    }
}

/* VGA functions */
void vga_putchar(char c) {
    if (c == '\n') {
        vga_offset += VGA_WIDTH - (vga_offset % VGA_WIDTH);
        return;
    }
    
    vga_buffer[vga_offset] = (uint16_t)c | (0x0F << 8); // White text on black bg
    vga_offset++;
}

void vga_print(const char *str) {
    while (*str) {
        vga_putchar(*str++);
    }
}

void kernel_main(void) {
    serial_init();
    
    serial_print("\n");
    serial_print("=================================\n");
    serial_print("      Welcome to NexusOS\n");
    serial_print("=================================\n");
    serial_print("Kernel loaded successfully.\n");
    serial_print("System initialized.\n");
    serial_print("Running on x86-64 bare metal.\n");
    serial_print("=================================\n");
    
    vga_print("NexusOS\n");
    vga_print("Kernel Ready\n");
    
    // Halt
    while (1) {
        __asm__("hlt");
    }
}
