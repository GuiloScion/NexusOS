/* gfx.c -- surface drawing primitives. See gfx.h. */

#include "gfx.h"
#include "font8x8.h"
#include "string.h"

void gfx_pixel(const surface_t *s, int x, int y, uint32_t color) {
    if (x < 0 || y < 0 || (uint32_t)x >= s->width || (uint32_t)y >= s->height) return;
    uint8_t *p = s->addr + (uint64_t)y * s->pitch + (uint64_t)x * (s->bpp / 8);
    if (s->bpp == 32) {
        *(uint32_t *)p = color;
    } else {
        p[0] = (uint8_t)(color & 0xFF);
        p[1] = (uint8_t)((color >> 8) & 0xFF);
        p[2] = (uint8_t)((color >> 16) & 0xFF);
    }
}

void gfx_fill(const surface_t *s, int x, int y, int w, int h, uint32_t color) {
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w, y1 = y + h;
    if (x1 > (int)s->width)  x1 = (int)s->width;
    if (y1 > (int)s->height) y1 = (int)s->height;
    int bytes = s->bpp / 8;
    for (int yy = y0; yy < y1; yy++) {
        uint8_t *row = s->addr + (uint64_t)yy * s->pitch + (uint64_t)x0 * bytes;
        if (s->bpp == 32) {
            uint32_t *px = (uint32_t *)row;
            for (int xx = x0; xx < x1; xx++) *px++ = color;
        } else {
            uint8_t *px = row;
            for (int xx = x0; xx < x1; xx++) {
                px[0] = (uint8_t)(color & 0xFF);
                px[1] = (uint8_t)((color >> 8) & 0xFF);
                px[2] = (uint8_t)((color >> 16) & 0xFF);
                px += 3;
            }
        }
    }
}

void gfx_blit(const surface_t *dst, const surface_t *src) {
    if (dst->bpp == src->bpp && dst->pitch == src->pitch &&
        dst->width == src->width && dst->height == src->height) {
        memcpy(dst->addr, src->addr, (uint64_t)src->pitch * src->height);
        return;
    }
    uint32_t w = src->width  < dst->width  ? src->width  : dst->width;
    uint32_t h = src->height < dst->height ? src->height : dst->height;
    uint32_t row_bytes = w * (src->bpp / 8);
    for (uint32_t y = 0; y < h; y++)
        memcpy(dst->addr + (uint64_t)y * dst->pitch,
               src->addr + (uint64_t)y * src->pitch, row_bytes);
}

void gfx_glyph(const surface_t *s, int x, int y, char c,
               uint32_t fg, uint32_t bg, int scale, bool opaque) {
    const uint8_t *g = font8x8_basic[(unsigned char)c & 0x7F];
    for (int row = 0; row < 8; row++) {
        uint8_t bits = g[row];
        for (int col = 0; col < 8; col++) {
            if (bits & (1 << col))
                gfx_fill(s, x + col * scale, y + row * scale, scale, scale, fg);
            else if (opaque)
                gfx_fill(s, x + col * scale, y + row * scale, scale, scale, bg);
        }
    }
}

void gfx_text(const surface_t *s, int x, int y, const char *str,
              uint32_t fg, int scale) {
    int cx = x;
    for (; *str; str++) {
        gfx_glyph(s, cx, y, *str, fg, 0, scale, false);
        cx += 8 * scale;
    }
}
