/* wm.h -- window manager + compositor.
 *
 * Owns an off-screen back buffer. A compositor task composes the scene
 * (desktop background -> text console -> windows, in z-order) into the back
 * buffer whenever something changes, then presents it (blit to the screen +
 * a mouse-cursor overlay on top) every frame. Mouse clicks raise/focus
 * windows; dragging a title bar moves a window.
 */
#ifndef NEXUS_WM_H
#define NEXUS_WM_H

#include "types.h"

/* Returns the window id, or -1 on failure. body height excludes the title bar. */
int  wm_add_window(int x, int y, int w, int body_h, const char *title,
                   const char *const *lines, int nlines);

void wm_init(void);   /* allocates the back buffer, adds demo windows, starts the compositor */

#endif
