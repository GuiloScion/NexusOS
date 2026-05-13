/* keyboard.c
 *
 * PS/2 controller data port = 0x60, status port = 0x64.
 * Each IRQ1 means a byte is waiting at 0x60.
 *
 * Scancode set 1: pressing a key produces 0x01..0x58 (make code);
 * releasing OR's in 0x80 (break code). Extended keys (arrows, etc.)
 * are prefixed with 0xE0 and ignored here.
 */

#include "keyboard.h"
#include "idt.h"
#include "io.h"
#include "pic.h"

#define KBD_DATA    0x60

/* US QWERTY layout, scancode set 1. 0 means "no ASCII for this code." */
static const char map_unshifted[128] = {
    0,   0x1B, '1', '2', '3', '4', '5', '6', '7', '8',  /* 0x00 - 0x09 */
    '9', '0', '-', '=', '\b','\t','q', 'w', 'e', 'r',   /* 0x0A - 0x13 */
    't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,    /* 0x14 - 0x1D */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',   /* 0x1E - 0x27 */
    '\'','`', 0,   '\\','z', 'x', 'c', 'v', 'b', 'n',   /* 0x28 - 0x31 */
    'm', ',', '.', '/', 0,   '*', 0,   ' ', 0,   0,     /* 0x32 - 0x3B */
};

static const char map_shifted[128] = {
    0,   0x1B, '!', '@', '#', '$', '%', '^', '&', '*',
    '(', ')', '_', '+', '\b','\t','Q', 'W', 'E', 'R',
    'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
    '"', '~', 0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N',
    'M', '<', '>', '?', 0,   '*', 0,   ' ', 0,   0,
};

#define RING_SIZE   256
static volatile char     ring[RING_SIZE];
static volatile uint32_t ring_head;       /* write index (IRQ) */
static volatile uint32_t ring_tail;       /* read index (consumer) */

static bool shift_down;
static bool ext_prefix;
static bool caps_lock;

static void ring_push(char c) {
    uint32_t next = (ring_head + 1) % RING_SIZE;
    if (next == ring_tail) return;   /* full -- drop */
    ring[ring_head] = c;
    ring_head = next;
}

static void keyboard_irq(interrupt_frame_t *frame) {
    (void)frame;
    uint8_t sc = inb(KBD_DATA);

    if (sc == 0xE0) { ext_prefix = true; return; }
    if (ext_prefix) { ext_prefix = false; return; }

    bool released = (sc & 0x80) != 0;
    uint8_t code  = sc & 0x7F;

    /* Modifier handling. */
    if (code == 0x2A || code == 0x36) {     /* L-Shift / R-Shift */
        shift_down = !released;
        return;
    }
    if (!released && code == 0x3A) {        /* Caps Lock make */
        caps_lock = !caps_lock;
        return;
    }
    if (released) return;

    if (code >= 128) return;
    char c = shift_down ? map_shifted[code] : map_unshifted[code];

    /* Caps Lock only affects letters. */
    if (caps_lock && c >= 'a' && c <= 'z') c = (char)(c - 32);
    else if (caps_lock && shift_down && c >= 'A' && c <= 'Z') c = (char)(c + 32);

    if (c) ring_push(c);
}

void keyboard_init(void) {
    /* Drain any pending byte the BIOS left behind. */
    while (inb(0x64) & 0x01) (void)inb(KBD_DATA);

    irq_register(1, keyboard_irq);
    pic_unmask(1);
}

bool keyboard_try_getc(char *out) {
    if (ring_head == ring_tail) return false;
    *out = ring[ring_tail];
    ring_tail = (ring_tail + 1) % RING_SIZE;
    return true;
}

char keyboard_getc(void) {
    for (;;) {
        char c;
        if (keyboard_try_getc(&c)) return c;
        __asm__ volatile ("sti; hlt");
    }
}
