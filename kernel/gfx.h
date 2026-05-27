/* gfx.h -- surface drawing primitives.
 *
 * A surface_t is any rectangular pixel buffer (the hardware framebuffer, an
 * off-screen back buffer, etc.). All helpers take a surface so the same code
 * draws to the screen or to an off-screen buffer for compositing. Colors are
 * 0x00RRGGBB; conversion to the surface's pixel format is handled here.
 */
#ifndef NEXUS_GFX_H
#define NEXUS_GFX_H

#include "types.h"

typedef struct {
    uint8_t  *addr;
    uint32_t  pitch;    /* bytes per scanline */
    uint32_t  width;
    uint32_t  height;
    uint32_t  bpp;      /* 24 or 32 */
} surface_t;

void gfx_pixel(const surface_t *s, int x, int y, uint32_t color);
void gfx_fill (const surface_t *s, int x, int y, int w, int h, uint32_t color);

/* Copy src over dst. Fast path when geometry matches; otherwise clipped. */
void gfx_blit(const surface_t *dst, const surface_t *src);

/* One 8x8 glyph, scaled by `scale`. If `opaque`, the background cells are
 * filled with `bg`; otherwise only set pixels are drawn (transparent). */
void gfx_glyph(const surface_t *s, int x, int y, char c,
               uint32_t fg, uint32_t bg, int scale, bool opaque);

/* A NUL-terminated string, left to right, transparent background. */
void gfx_text(const surface_t *s, int x, int y, const char *str,
              uint32_t fg, int scale);

static inline uint32_t gfx_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

#endif
