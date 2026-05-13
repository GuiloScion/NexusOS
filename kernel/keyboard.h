/* keyboard.h -- PS/2 keyboard on IRQ1.
 *
 * Decodes scancode set 1 into ASCII. Non-printable keys, modifiers,
 * function keys, and extended (0xE0-prefixed) sequences are dropped.
 * A ring buffer holds up to 256 characters between IRQ and reader.
 */
#ifndef NEXUS_KEYBOARD_H
#define NEXUS_KEYBOARD_H

#include "types.h"

void keyboard_init(void);
bool keyboard_try_getc(char *out);   /* non-blocking, returns false if empty */
char keyboard_getc(void);            /* blocking with hlt-spin */

#endif
