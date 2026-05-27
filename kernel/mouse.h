/* mouse.h -- PS/2 mouse driver + software cursor.
 *
 * Initializes the 8042's auxiliary device, parses 3-byte PS/2 packets on
 * IRQ12, tracks an absolute cursor position (clamped to the framebuffer) and
 * the button state, and draws an arrow cursor on the framebuffer.
 */
#ifndef NEXUS_MOUSE_H
#define NEXUS_MOUSE_H

#include "types.h"

#define MOUSE_BTN_LEFT   0x01
#define MOUSE_BTN_RIGHT  0x02
#define MOUSE_BTN_MIDDLE 0x04

void    mouse_init(void);
int32_t mouse_x(void);
int32_t mouse_y(void);
uint8_t mouse_buttons(void);

#endif
