/* fbcon.c -- text console as a character grid. See fbcon.h. */

#include "fbcon.h"
#include "fb.h"
#include "gfx.h"
#include "string.h"

#define SCALE   2
#define CELL_W  (8 * SCALE)
#define CELL_H  (8 * SCALE)
#define TAB     4

#define COLS_MAX 128
#define ROWS_MAX 64

static bool     ready;
static uint32_t cols, rows;
static uint32_t cur_col, cur_row;
static uint32_t fg, bg;
static char     grid[ROWS_MAX][COLS_MAX];
static uint64_t version;

bool fbcon_init(void) {
    if (!fb_active()) { ready = false; return false; }
    const framebuffer_t *f = fb_get();
    cols = f->width  / CELL_W; if (cols > COLS_MAX) cols = COLS_MAX;
    rows = f->height / CELL_H; if (rows > ROWS_MAX) rows = ROWS_MAX;
    fg = gfx_rgb(220, 220, 220);
    bg = gfx_rgb(16, 16, 28);
    cur_col = cur_row = 0;
    for (uint32_t r = 0; r < ROWS_MAX; r++)
        for (uint32_t c = 0; c < COLS_MAX; c++) grid[r][c] = ' ';
    ready = true;
    version++;
    return true;
}

bool     fbcon_ready(void)   { return ready; }
uint64_t fbcon_version(void) { return version; }
uint32_t fbcon_bg(void)      { return bg; }

void fbcon_set_colors(uint32_t f, uint32_t b) { fg = f; bg = b; version++; }

void fbcon_clear(void) {
    if (!ready) return;
    for (uint32_t r = 0; r < ROWS_MAX; r++)
        for (uint32_t c = 0; c < COLS_MAX; c++) grid[r][c] = ' ';
    cur_col = cur_row = 0;
    version++;
}

static void scroll(void) {
    for (uint32_t r = 1; r < rows; r++)
        memcpy(grid[r - 1], grid[r], COLS_MAX);
    for (uint32_t c = 0; c < COLS_MAX; c++) grid[rows - 1][c] = ' ';
    cur_row = rows - 1;
}

static void newline(void) {
    cur_col = 0;
    if (++cur_row >= rows) scroll();
}

void fbcon_putc(char c) {
    if (!ready) return;
    version++;
    switch (c) {
        case '\n': newline();                              return;
        case '\r': cur_col = 0;                            return;
        case '\t': do { fbcon_putc(' '); } while (cur_col % TAB); return;
        case '\b':
            if (cur_col > 0) { cur_col--; grid[cur_row][cur_col] = ' '; }
            return;
        default: break;
    }
    if ((unsigned char)c < 0x20) return;
    grid[cur_row][cur_col] = c;
    if (++cur_col >= cols) newline();
}

void fbcon_render(const surface_t *dst) {
    if (!ready) return;
    for (uint32_t r = 0; r < rows; r++) {
        for (uint32_t c = 0; c < cols; c++) {
            char ch = grid[r][c];
            if (ch != ' ' && ch != '\0')
                gfx_glyph(dst, (int)(c * CELL_W), (int)(r * CELL_H), ch, fg, bg, SCALE, false);
        }
    }
}
