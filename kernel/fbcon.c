/* fbcon.c -- framebuffer text console. See fbcon.h. */

#include "fbcon.h"
#include "fb.h"
#include "font8x8.h"
#include "string.h"

#define SCALE   2
#define CELL_W  (8 * SCALE)
#define CELL_H  (8 * SCALE)
#define TAB     4

static bool     ready;
static uint32_t cols, rows;        /* console size in character cells */
static uint32_t cur_col, cur_row;
static uint32_t fg, bg;

static void draw_glyph(unsigned char ch, uint32_t cx, uint32_t cy) {
    const uint8_t *g = font8x8_basic[ch & 0x7F];
    uint32_t x0 = cx * CELL_W;
    uint32_t y0 = cy * CELL_H;
    for (uint32_t row = 0; row < 8; row++) {
        uint8_t bits = g[row];
        for (uint32_t col = 0; col < 8; col++) {
            uint32_t color = (bits >> col) & 1 ? fg : bg;
            fb_fill_rect(x0 + col * SCALE, y0 + row * SCALE, SCALE, SCALE, color);
        }
    }
}

static void erase_cell(uint32_t cx, uint32_t cy) {
    fb_fill_rect(cx * CELL_W, cy * CELL_H, CELL_W, CELL_H, bg);
}

static void scroll(void) {
    const framebuffer_t *f = fb_get();
    uint64_t line_bytes = (uint64_t)CELL_H * f->pitch;
    /* Shift everything up by one text row (forward copy: dst < src is safe). */
    memcpy(f->addr, f->addr + line_bytes, (uint64_t)(f->height - CELL_H) * f->pitch);
    /* Clear the now-vacated bottom text row. */
    fb_fill_rect(0, (rows - 1) * CELL_H, f->width, CELL_H, bg);
    cur_row = rows - 1;
}

static void newline(void) {
    cur_col = 0;
    if (++cur_row >= rows) scroll();
}

bool fbcon_init(void) {
    if (!fb_active()) { ready = false; return false; }
    const framebuffer_t *f = fb_get();
    cols = f->width  / CELL_W;
    rows = f->height / CELL_H;
    fg = fb_rgb(220, 220, 220);
    bg = fb_rgb(16, 16, 28);
    cur_col = cur_row = 0;
    fb_clear(bg);
    ready = true;
    return true;
}

bool fbcon_ready(void) { return ready; }

void fbcon_set_colors(uint32_t f, uint32_t b) { fg = f; bg = b; }

void fbcon_clear(void) {
    if (!ready) return;
    fb_clear(bg);
    cur_col = cur_row = 0;
}

void fbcon_putc(char c) {
    if (!ready) return;
    switch (c) {
        case '\n': newline();                       return;
        case '\r': cur_col = 0;                      return;
        case '\t':
            do { fbcon_putc(' '); } while (cur_col % TAB);
            return;
        case '\b':
            if (cur_col > 0) { cur_col--; erase_cell(cur_col, cur_row); }
            return;
        default: break;
    }
    draw_glyph((unsigned char)c, cur_col, cur_row);
    if (++cur_col >= cols) newline();
}
