/* fb.c -- linear-framebuffer graphics output. See fb.h. */

#include "fb.h"
#include "vmm.h"
#include "console.h"

/* Descriptor the bootloader leaves in low memory after VBE setup. */
#define FB_INFO_ADDR 0x9700

static framebuffer_t fb;

bool fb_init(void) {
    volatile uint32_t *info = (volatile uint32_t *)(uintptr_t)FB_INFO_ADDR;

    uint32_t lfb    = info[0];
    uint32_t pitch  = info[1];
    uint32_t width  = info[2];
    uint32_t height = info[3];
    uint32_t bpp    = info[4];
    uint32_t valid  = info[5];

    if (!valid || lfb == 0 || width == 0 || height == 0) {
        fb.active = false;
        return false;
    }

    /* The framebuffer lives above identity-mapped RAM, so map it in
     * explicitly. Identity-map (virt == phys) one page at a time. */
    uint64_t size = (uint64_t)pitch * height;
    for (uint64_t off = 0; off < size; off += 0x1000) {
        if (!vmm_map((uintptr_t)lfb + off, (uintptr_t)lfb + off, PTE_KERNEL_RW)) {
            fb.active = false;
            return false;
        }
    }

    fb.addr   = (uint8_t *)(uintptr_t)lfb;
    fb.pitch  = pitch;
    fb.width  = width;
    fb.height = height;
    fb.bpp    = bpp;
    fb.active = true;
    return true;
}

bool fb_active(void) { return fb.active; }

const framebuffer_t *fb_get(void) { return &fb; }

void fb_putpixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!fb.active || x >= fb.width || y >= fb.height) return;
    uint8_t *p = fb.addr + (uint64_t)y * fb.pitch + (uint64_t)x * (fb.bpp / 8);
    if (fb.bpp == 32) {
        *(uint32_t *)p = color;
    } else {                       /* 24 bpp */
        p[0] = (uint8_t)(color & 0xFF);
        p[1] = (uint8_t)((color >> 8) & 0xFF);
        p[2] = (uint8_t)((color >> 16) & 0xFF);
    }
}

void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    if (!fb.active) return;
    uint32_t x1 = x + w, y1 = y + h;
    if (x1 > fb.width)  x1 = fb.width;
    if (y1 > fb.height) y1 = fb.height;

    for (uint32_t yy = y; yy < y1; yy++) {
        uint8_t *row = fb.addr + (uint64_t)yy * fb.pitch + (uint64_t)x * (fb.bpp / 8);
        if (fb.bpp == 32) {
            uint32_t *px = (uint32_t *)row;
            for (uint32_t xx = x; xx < x1; xx++) *px++ = color;
        } else {
            uint8_t *px = row;
            for (uint32_t xx = x; xx < x1; xx++) {
                px[0] = (uint8_t)(color & 0xFF);
                px[1] = (uint8_t)((color >> 8) & 0xFF);
                px[2] = (uint8_t)((color >> 16) & 0xFF);
                px += 3;
            }
        }
    }
}

void fb_clear(uint32_t color) {
    fb_fill_rect(0, 0, fb.width, fb.height, color);
}
