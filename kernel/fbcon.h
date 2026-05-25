/* fbcon.h -- text console rendered on the linear framebuffer.
 *
 * Draws an 8x8 bitmap font (scaled) into the framebuffer, tracking a cursor
 * in character cells, with newline / carriage-return / backspace / tab and
 * scroll-on-overflow. Once fbcon_init() succeeds, console_putc() also routes
 * output here, so the kernel shell becomes visible on screen.
 */
#ifndef NEXUS_FBCON_H
#define NEXUS_FBCON_H

#include "types.h"

bool fbcon_init(void);                       /* needs an active framebuffer */
bool fbcon_ready(void);
void fbcon_putc(char c);
void fbcon_clear(void);
void fbcon_set_colors(uint32_t fg, uint32_t bg);

#endif
