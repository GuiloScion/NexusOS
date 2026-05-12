#define VGA_ADDRESS 0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long uint64_t;

static uint16_t *vga_buffer = (uint16_t *)VGA_ADDRESS;
static uint32_t vga_offset = 0;

void kernel_putchar(char c) {
    if (c == '\n') {
        vga_offset += VGA_WIDTH - (vga_offset % VGA_WIDTH);
        return;
    }
    
    vga_buffer[vga_offset] = (uint16_t)c | (0x0F << 8); // White text on black bg
    vga_offset++;
}

void kernel_print(const char *str) {
    while (*str) {
        kernel_putchar(*str++);
    }
}

void kernel_main(void) {
    kernel_print("Welcome to Bare Metal OS!\n");
    kernel_print("Kernel loaded successfully.\n");
    
    // Halt
    while (1) {
        __asm__("hlt");
    }
}
