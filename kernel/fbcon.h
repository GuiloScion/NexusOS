/* fbcon.h -- text console as a character grid.
 *
 * fbcon no longer draws directly to the screen. console_putc() feeds
 * characters in, which update an in-memory grid (with scrolling); the
 * compositor calls fbcon_render() to paint the grid into a surface. A version
 * counter lets the compositor skip re-rendering when nothing changed.
 */
#ifndef NEXUS_FBCON_H
#define NEXUS_FBCON_H

#include "types.h"
#include "gfx.h"

bool     fbcon_init(uint32_t cols, uint32_t rows);   /* grid size in cells */
bool     fbcon_ready(void);
void     fbcon_putc(char c);
void     fbcon_clear(void);
void     fbcon_set_colors(uint32_t fg, uint32_t bg);

uint64_t fbcon_version(void);              /* bumped on every change      */
uint32_t fbcon_bg(void);                   /* console background color    */
uint32_t fbcon_cell(void);                 /* cell size in pixels         */
void     fbcon_cols_rows(uint32_t *cols, uint32_t *rows);

/* Render the grid into `dst` with its top-left at pixel (ox, oy). */
void     fbcon_render(const surface_t *dst, int ox, int oy);

#endif
