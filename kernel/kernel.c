/* kernel.c -- NexusOS top-level entry.
 *
 * Boot order:
 *   1. console      (so everything else can print)
 *   2. idt          (install handlers BEFORE enabling interrupts)
 *   3. pic          (remap IRQs to 0x20..0x2F)
 *   4. pit          (100 Hz tick on IRQ0)
 *   5. keyboard     (IRQ1)
 *   6. sti          (interrupts on)
 *   7. pmm          (uses E820 from bootloader at 0x9000)
 *   8. vmm          (rebuilds page tables with 4 KiB API)
 *   9. kmalloc      (heap, demand-paged from PMM via VMM)
 */

#include "console.h"
#include "idt.h"
#include "pic.h"
#include "pit.h"
#include "keyboard.h"
#include "pmm.h"
#include "vmm.h"
#include "kmalloc.h"
#include "io.h"

extern uint8_t __kernel_end;        /* from linker.ld */

static uint64_t e820_ram_end(void) {
    uint32_t      count   = *(volatile uint32_t *)E820_COUNT_ADDR;
    e820_entry_t *entries = (e820_entry_t *)E820_ENTRIES_ADDR;
    uint64_t      end     = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (entries[i].type != 1) continue;
        uint64_t top = entries[i].base + entries[i].length;
        if (top > end) end = top;
    }
    return end;
}

static void banner(void) {
    console_puts("\n");
    console_puts("==========================================\n");
    console_puts("           NexusOS  -  x86_64\n");
    console_puts("==========================================\n");
}

static void shell(void) {
    console_puts("\nnexus> ");
    char line[128];
    uint32_t n = 0;
    for (;;) {
        char c = keyboard_getc();
        if (c == '\n') {
            console_putc('\n');
            line[n] = '\0';
            if (n == 0) { /* nothing */ }
            else if (n == 4 && line[0]=='h' && line[1]=='e' && line[2]=='l' && line[3]=='p') {
                console_puts("commands: help, ticks, mem, halt\n");
            } else if (n == 5 && line[0]=='t' && line[1]=='i' && line[2]=='c' && line[3]=='k' && line[4]=='s') {
                console_puts("ticks: "); console_put_dec(pit_ticks()); console_putc('\n');
            } else if (n == 3 && line[0]=='m' && line[1]=='e' && line[2]=='m') {
                uint64_t used, freeb;
                kmalloc_stats(&used, &freeb);
                console_puts("frames used: "); console_put_dec(pmm_used_frames());
                console_puts(" / ");           console_put_dec(pmm_total_frames());
                console_puts("\nheap used:  "); console_put_dec(used);
                console_puts(" B, free: ");    console_put_dec(freeb); console_puts(" B\n");
            } else if (n == 4 && line[0]=='h' && line[1]=='a' && line[2]=='l' && line[3]=='t') {
                console_puts("halting.\n");
                for (;;) { cli(); hlt(); }
            } else {
                console_puts("unknown command. try: help\n");
            }
            n = 0;
            console_puts("nexus> ");
        } else if (c == '\b') {
            if (n > 0) { n--; console_puts("\b \b"); }
        } else if (n + 1 < sizeof(line)) {
            line[n++] = c;
            console_putc(c);
        }
    }
}

void kernel_main(void) {
    console_init();
    banner();
    console_puts("[boot] console ready\n");

    idt_init();
    console_puts("[boot] idt installed\n");

    pic_init();
    console_puts("[boot] pic remapped to 0x20\n");

    pit_init(100);
    keyboard_init();
    sti();
    console_puts("[boot] interrupts enabled\n");

    uintptr_t kend = (uintptr_t)&__kernel_end;
    console_puts("[boot] kernel ends at "); console_put_hex(kend); console_putc('\n');

    pmm_init(kend);

    uint64_t ram_end = e820_ram_end();
    vmm_init(ram_end);

    kmalloc_init();

    /* Sanity-check the heap. */
    void *a = kmalloc(64);
    void *b = kmalloc(128);
    void *c = kmalloc(4096);
    console_puts("[test] kmalloc -> ");
    console_put_hex((uintptr_t)a); console_putc(' ');
    console_put_hex((uintptr_t)b); console_putc(' ');
    console_put_hex((uintptr_t)c); console_putc('\n');
    kfree(b); kfree(a); kfree(c);

    console_puts("\nNexusOS ready. Type 'help' for commands.\n");
    shell();
}
