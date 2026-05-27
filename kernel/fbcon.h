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

bool     fbcon_init(void);                 /* needs an active framebuffer */
bool     fbcon_ready(void);
void     fbcon_putc(char c);
void     fbcon_clear(void);
void     fbcon_set_colors(uint32_t fg, uint32_t bg);

uint64_t fbcon_version(void);              /* bumped on every change      */
uint32_t fbcon_bg(void);                   /* console background color    */
void     fbcon_render(const surface_t *dst);

#endif
