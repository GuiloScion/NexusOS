/* wm.c -- window manager + compositor. See wm.h. */

#include "wm.h"
#include "gfx.h"
#include "fb.h"
#include "fbcon.h"
#include "mouse.h"
#include "sched.h"
#include "kmalloc.h"
#include "console.h"

#define MAX_WINDOWS  8
#define TITLE_H      24
#define BORDER       2
#define LINE_H       18
#define TXT_SCALE    2

typedef struct {
    int  x, y, w, h;            /* h is body height (title bar is extra) */
    const char *title;
    const char *const *lines;
    int  nlines;
    bool used;
} window_t;

static surface_t screen;
static surface_t back;
static bool      active;

static window_t  wins[MAX_WINDOWS];
static int       zorder[MAX_WINDOWS];   /* indices, front-most last */
static int       nwins;

static volatile bool layout_dirty;
static uint64_t      last_console_ver;

/* colors */
static uint32_t COL_BORDER, COL_TITLE_A, COL_TITLE_I, COL_BODY, COL_BODY_TXT, COL_TITLE_TXT;

/* ---- arrow cursor bitmap ------------------------------------------------ */
#define CUR_H 19
static const char *const arrow[CUR_H] = {
    "X", "XX", "X.X", "X..X", "X...X", "X....X", "X.....X", "X......X",
    "X.......X", "X........X", "X.........X", "X......XXXXX", "X...X..X",
    "X..XX..X", "X.X  X..X", "XX   X..X", "X     X..X", "      X..X", "       XX",
};

static void draw_cursor(int mx, int my) {
    for (int r = 0; r < CUR_H; r++) {
        const char *row = arrow[r];
        for (int c = 0; row[c]; c++) {
            if (row[c] == 'X')      gfx_pixel(&screen, mx + c, my + r, 0x000000);
            else if (row[c] == '.') gfx_pixel(&screen, mx + c, my + r, 0xFFFFFF);
        }
    }
}

/* ---- window helpers ----------------------------------------------------- */

static int win_total_h(const window_t *w) { return TITLE_H + w->h; }

static void draw_window(const window_t *w, bool focused) {
    int th = win_total_h(w);
    gfx_fill(&back, w->x - BORDER, w->y - BORDER,
             w->w + 2 * BORDER, th + 2 * BORDER, COL_BORDER);
    gfx_fill(&back, w->x, w->y, w->w, TITLE_H, focused ? COL_TITLE_A : COL_TITLE_I);
    gfx_text(&back, w->x + 6, w->y + 4, w->title, COL_TITLE_TXT, TXT_SCALE);
    gfx_fill(&back, w->x, w->y + TITLE_H, w->w, w->h, COL_BODY);
    for (int i = 0; i < w->nlines; i++)
        gfx_text(&back, w->x + 8, w->y + TITLE_H + 8 + i * LINE_H,
                 w->lines[i], COL_BODY_TXT, TXT_SCALE);
}

int wm_add_window(int x, int y, int w, int body_h, const char *title,
                  const char *const *lines, int nlines) {
    if (nwins >= MAX_WINDOWS) return -1;
    int id = nwins;
    wins[id] = (window_t){ .x = x, .y = y, .w = w, .h = body_h,
                           .title = title, .lines = lines, .nlines = nlines,
                           .used = true };
    zorder[nwins] = id;
    nwins++;
    layout_dirty = true;
    return id;
}

/* topmost window whose bounds contain (px,py), or -1 */
static int window_at(int px, int py) {
    for (int i = nwins - 1; i >= 0; i--) {
        const window_t *w = &wins[zorder[i]];
        if (px >= w->x - BORDER && px < w->x + w->w + BORDER &&
            py >= w->y - BORDER && py < w->y + win_total_h(w) + BORDER)
            return i;                       /* return z-order slot */
    }
    return -1;
}

static void raise_slot(int slot) {
    if (slot < 0 || slot == nwins - 1) return;
    int id = zorder[slot];
    for (int i = slot; i < nwins - 1; i++) zorder[i] = zorder[i + 1];
    zorder[nwins - 1] = id;
    layout_dirty = true;
}

/* ---- input -------------------------------------------------------------- */

static void poll_input(void) {
    static bool prev_left;
    static bool dragging;
    static int  drag_id, drag_dx, drag_dy;

    int  mx   = mouse_x();
    int  my   = mouse_y();
    bool left = (mouse_buttons() & MOUSE_BTN_LEFT) != 0;

    if (left && !prev_left) {                /* press edge: focus/raise */
        int slot = window_at(mx, my);
        if (slot >= 0) {
            int id = zorder[slot];
            raise_slot(slot);
            const window_t *w = &wins[id];
            if (my < w->y + TITLE_H) {        /* grabbed the title bar */
                dragging = true; drag_id = id;
                drag_dx = mx - w->x; drag_dy = my - w->y;
            }
        }
    }
    if (left && dragging) {
        wins[drag_id].x = mx - drag_dx;
        wins[drag_id].y = my - drag_dy;
        layout_dirty = true;
    }
    if (!left) dragging = false;
    prev_left = left;
}

/* ---- compose / present -------------------------------------------------- */

static void compose(void) {
    gfx_fill(&back, 0, 0, (int)back.width, (int)back.height, fbcon_bg());
    fbcon_render(&back);
    for (int i = 0; i < nwins; i++)
        draw_window(&wins[zorder[i]], i == nwins - 1);
}

static void present(void) {
    gfx_blit(&screen, &back);
    draw_cursor(mouse_x(), mouse_y());
}

static void compositor_task(void) {
    for (;;) {
        poll_input();
        uint64_t v = fbcon_version();
        if (layout_dirty || v != last_console_ver) {
            compose();
            last_console_ver = v;
            layout_dirty = false;
        }
        present();
        sched_sleep_ms(16);
    }
}

/* ---- init --------------------------------------------------------------- */

static const char *const w_welcome[] = {
    "Welcome to NexusOS.",
    "",
    "A from-scratch x86-64 OS",
    "with a compositing window",
    "manager. Drag me by my",
    "title bar; click to raise.",
};
static const char *const w_system[] = {
    "x86-64 long mode",
    "Preemptive scheduler",
    "FAT12 storage",
    "VESA framebuffer 1024x768",
    "PS/2 keyboard + mouse",
};
static const char *const w_shell[] = {
    "The text shell runs as the",
    "desktop background, behind",
    "these windows. Try typing",
    "'help' (serial or screen).",
};

void wm_init(void) {
    if (!fb_active()) return;
    const framebuffer_t *f = fb_get();

    screen = (surface_t){ f->addr, f->pitch, f->width, f->height, f->bpp };

    uint64_t sz = (uint64_t)f->pitch * f->height;
    uint8_t *mem = (uint8_t *)kmalloc(sz);
    if (!mem) { console_puts("[wm] back buffer alloc failed\n"); return; }
    back = (surface_t){ mem, f->pitch, f->width, f->height, f->bpp };

    COL_BORDER    = gfx_rgb(70, 70, 95);
    COL_TITLE_A   = gfx_rgb(60, 95, 205);
    COL_TITLE_I   = gfx_rgb(75, 78, 95);
    COL_BODY      = gfx_rgb(238, 238, 244);
    COL_BODY_TXT  = gfx_rgb(24, 24, 34);
    COL_TITLE_TXT = gfx_rgb(245, 245, 250);

    wm_add_window(120,  90, 360, 6 * LINE_H + 16, "Welcome",
                  w_welcome, 6);
    wm_add_window(540, 150, 360, 5 * LINE_H + 16, "System",
                  w_system, 5);
    wm_add_window(300, 380, 380, 4 * LINE_H + 16, "Shell",
                  w_shell, 4);

    active = true;
    task_create("compositor", compositor_task);
    console_puts("[wm] compositor started\n");
}
