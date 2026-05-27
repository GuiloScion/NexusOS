/* mouse.c -- PS/2 mouse driver + software arrow cursor. See mouse.h.
 *
 * The mouse hangs off the 8042 "second" PS/2 port: commands are sent by
 * prefixing 0xD4 to the controller, and completed packets arrive on IRQ12
 * (slave PIC). Each packet is 3 bytes:
 *   byte0: [Y-overflow][X-overflow][Y-sign][X-sign][1][middle][right][left]
 *   byte1: X movement (9-bit two's complement, sign from byte0)
 *   byte2: Y movement (9-bit, +Y is up, so screen Y moves the other way)
 *
 * The cursor is drawn directly into the framebuffer with save-under-restore.
 * Because there is no compositor yet, text drawn underneath the cursor can be
 * clobbered when the cursor next moves -- that goes away once a window manager
 * owns the screen (roadmap step 4).
 */

#include "mouse.h"
#include "idt.h"
#include "io.h"
#include "pic.h"
#include "fb.h"
#include "console.h"

#define PS2_DATA    0x60
#define PS2_STATUS  0x64        /* read */
#define PS2_CMD     0x64        /* write */

#define ST_OUTPUT_FULL  0x01
#define ST_INPUT_FULL   0x02

/* ---- arrow cursor bitmap: 'X' outline, '.' fill, ' '/end = transparent --- */
#define CURSOR_W 12
#define CURSOR_H 19
static const char *const arrow[CURSOR_H] = {
    "X",
    "XX",
    "X.X",
    "X..X",
    "X...X",
    "X....X",
    "X.....X",
    "X......X",
    "X.......X",
    "X........X",
    "X.........X",
    "X......XXXXX",
    "X...X..X",
    "X..XX..X",
    "X.X  X..X",
    "XX   X..X",
    "X     X..X",
    "      X..X",
    "       XX",
};

static const uint32_t COL_BLACK = 0x000000;
static const uint32_t COL_WHITE = 0xFFFFFF;

static int32_t  mx, my;
static uint8_t  buttons;

static uint8_t  packet[3];
static int      cycle;

static bool     cursor_on;
static int32_t  drawn_x, drawn_y;
static bool     saved_valid;
static uint32_t saved[CURSOR_H][CURSOR_W];

/* ---- 8042 polled access (used during init only) ------------------------- */

static void ps2_wait_input(void) {     /* wait until safe to write a byte */
    for (int i = 0; i < 200000; i++)
        if (!(inb(PS2_STATUS) & ST_INPUT_FULL)) return;
}
static void ps2_wait_output(void) {    /* wait until a byte is available */
    for (int i = 0; i < 200000; i++)
        if (inb(PS2_STATUS) & ST_OUTPUT_FULL) return;
}

static void mouse_cmd(uint8_t cmd) {
    ps2_wait_input(); outb(PS2_CMD, 0xD4);     /* "next byte -> mouse" */
    ps2_wait_input(); outb(PS2_DATA, cmd);
}
static uint8_t ps2_read(void) {
    ps2_wait_output();
    return inb(PS2_DATA);
}

/* ---- cursor ------------------------------------------------------------- */

static void cursor_save(void) {
    for (int r = 0; r < CURSOR_H; r++)
        for (int c = 0; c < CURSOR_W; c++)
            saved[r][c] = fb_getpixel((uint32_t)(drawn_x + c), (uint32_t)(drawn_y + r));
}
static void cursor_restore(void) {
    for (int r = 0; r < CURSOR_H; r++)
        for (int c = 0; c < CURSOR_W; c++)
            fb_putpixel((uint32_t)(drawn_x + c), (uint32_t)(drawn_y + r), saved[r][c]);
}
static void cursor_draw(void) {
    for (int r = 0; r < CURSOR_H; r++) {
        const char *row = arrow[r];
        for (int c = 0; row[c]; c++) {
            if (row[c] == 'X')      fb_putpixel((uint32_t)(drawn_x + c), (uint32_t)(drawn_y + r), COL_BLACK);
            else if (row[c] == '.') fb_putpixel((uint32_t)(drawn_x + c), (uint32_t)(drawn_y + r), COL_WHITE);
        }
    }
}

static void cursor_redraw(void) {
    if (!cursor_on) return;
    if (saved_valid) cursor_restore();
    drawn_x = mx; drawn_y = my;
    cursor_save();
    cursor_draw();
    saved_valid = true;
}

/* ---- IRQ ---------------------------------------------------------------- */

static void mouse_process(void) {
    if (packet[0] & 0xC0) return;          /* X/Y overflow -- drop packet */

    int dx = (int)packet[1] - (int)((packet[0] << 4) & 0x100);
    int dy = (int)packet[2] - (int)((packet[0] << 3) & 0x100);

    buttons = packet[0] & 0x07;

    mx += dx;
    my -= dy;                              /* screen Y grows downward */

    const framebuffer_t *f = fb_get();
    int32_t maxx = (int32_t)f->width  - 1;
    int32_t maxy = (int32_t)f->height - 1;
    if (mx < 0) mx = 0; else if (mx > maxx) mx = maxx;
    if (my < 0) my = 0; else if (my > maxy) my = maxy;

    cursor_redraw();
}

static void mouse_irq(interrupt_frame_t *frame) {
    (void)frame;
    uint8_t status = inb(PS2_STATUS);
    if (!(status & ST_OUTPUT_FULL)) { pic_send_eoi(12); return; }

    uint8_t data = inb(PS2_DATA);
    switch (cycle) {
        case 0:
            if (!(data & 0x08)) break;     /* bit3 always set -> resync */
            packet[0] = data; cycle = 1; break;
        case 1: packet[1] = data; cycle = 2; break;
        case 2: packet[2] = data; cycle = 0; mouse_process(); break;
    }
    pic_send_eoi(12);
}

/* ---- init --------------------------------------------------------------- */

void mouse_init(void) {
    cli();                                 /* keep keyboard IRQ off 0x60 during polled init */

    ps2_wait_input(); outb(PS2_CMD, 0xA8); /* enable auxiliary device */

    ps2_wait_input(); outb(PS2_CMD, 0x20); /* read controller config byte */
    uint8_t cfg = ps2_read();
    cfg |=  0x02;                          /* enable IRQ12 (second port) */
    cfg &= ~0x20;                          /* enable second-port clock    */
    ps2_wait_input(); outb(PS2_CMD, 0x60); /* write controller config byte */
    ps2_wait_input(); outb(PS2_DATA, cfg);

    mouse_cmd(0xF6); (void)ps2_read();     /* set defaults, eat ACK */
    mouse_cmd(0xF4); (void)ps2_read();     /* enable data reporting, eat ACK */

    irq_register(12, mouse_irq);
    pic_unmask(12);

    sti();

    if (fb_active()) {
        const framebuffer_t *f = fb_get();
        mx = (int32_t)f->width  / 2;
        my = (int32_t)f->height / 2;
        cursor_on = true;
        cursor_redraw();
    }
    console_puts("[mouse] PS/2 enabled, cursor at center\n");
}

int32_t mouse_x(void)       { return mx; }
int32_t mouse_y(void)       { return my; }
uint8_t mouse_buttons(void) { return buttons; }
