"""
make_cover.py — generate cover.png for Leanpub.

Output: 1800 x 2700 PNG, Leanpub's recommended cover dimensions.
Uses Cambria (Windows) for serif text and Consolas for the monospace
boot-log motif. Falls back to PIL's default font if a face is missing.
"""

import os
from PIL import Image, ImageDraw, ImageFont, ImageFilter


OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "cover.png")

W, H = 1800, 2700

# ---------------------------------------------------------------- palette
BG_TOP      = (16, 22, 32)        # deep slate at the very top
BG_BOTTOM   = (8, 12, 18)         # slightly darker at the bottom
TITLE_COL   = (240, 238, 228)     # warm off-white
SUB_COL     = (170, 165, 152)     # warm gray
RULE_COL    = (201, 167, 111)     # amber accent
LOG_DIM     = (110, 122, 138)     # cool grey for log brackets
LOG_BRIGHT  = (210, 220, 230)     # cool white for log message text
LOG_OK      = (180, 215, 165)     # subdued green for "ready" / "ok"
AUTHOR_COL  = (235, 232, 222)
SHELF_COL   = (95, 105, 120)      # subtle line decoration


# ---------------------------------------------------------------- fonts
WIN = "C:/Windows/Fonts"


def font(name, size):
    """Return an ImageFont, falling back to default on failure."""
    path = os.path.join(WIN, name)
    if os.path.exists(path):
        try:
            return ImageFont.truetype(path, size)
        except Exception:
            pass
    return ImageFont.load_default()


def sitka(size, variant):
    """Sitka variable font, specific named instance (Display Bold, Banner
    Bold, etc.). Sitka is Microsoft's high-quality book serif designed
    with multiple optical sizes; the Banner cut is meant for display use
    at large sizes and has the high contrast a cover needs.

    Falls back to Cambria Bold if Sitka cannot be loaded for any reason."""
    italic = variant.endswith("Italic")
    name = variant.replace(" Italic", "")
    path = os.path.join(
        WIN, "SitkaVF-Italic.ttf" if italic else "SitkaVF.ttf"
    )
    if os.path.exists(path):
        try:
            f = ImageFont.truetype(path, size)
            f.set_variation_by_name(name)
            return f
        except Exception:
            pass
    # Fallback
    return font("cambriab.ttf", size)


F_TITLE      = sitka(220, "Banner Bold")
F_SUB        = sitka(64,  "Heading Italic")
F_AUTHOR     = sitka(82,  "Display Bold")
F_AUTHOR_SUB = sitka(36,  "Text Italic")
F_OVERLINE   = sitka(36,  "Display Bold")
F_LOG        = font("consola.ttf", 44)
F_LOG_BOLD   = font("consolab.ttf", 44)


# ---------------------------------------------------------------- canvas
img = Image.new("RGB", (W, H), BG_TOP)
draw = ImageDraw.Draw(img)

# Vertical gradient background
for y in range(H):
    t = y / (H - 1)
    r = int(BG_TOP[0] * (1 - t) + BG_BOTTOM[0] * t)
    g = int(BG_TOP[1] * (1 - t) + BG_BOTTOM[1] * t)
    b = int(BG_TOP[2] * (1 - t) + BG_BOTTOM[2] * t)
    draw.line([(0, y), (W, y)], fill=(r, g, b))

# Subtle radial glow behind the title block (warm amber, very faint)
glow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
gdraw = ImageDraw.Draw(glow)
cx, cy = W // 2, 760
for radius in range(900, 100, -40):
    a = int(8 * (1.0 - radius / 900.0))   # very faint
    gdraw.ellipse(
        (cx - radius, cy - radius, cx + radius, cy + radius),
        fill=(201, 167, 111, max(0, a)),
    )
glow = glow.filter(ImageFilter.GaussianBlur(60))
img = Image.alpha_composite(img.convert("RGBA"), glow).convert("RGB")
draw = ImageDraw.Draw(img)


# ---------------------------------------------------------------- helpers
def text_w(s, f):
    bbox = draw.textbbox((0, 0), s, font=f)
    return bbox[2] - bbox[0]


def text_h(s, f):
    bbox = draw.textbbox((0, 0), s, font=f)
    return bbox[3] - bbox[1]


def draw_centered(s, f, y, color):
    w = text_w(s, f)
    draw.text(((W - w) // 2, y), s, font=f, fill=color)


# ---------------------------------------------------------------- overline
# Small caps marker above the title — sets genre/tone immediately.
overline = "A   HANDS-ON   BOOK"
draw_centered(overline, F_OVERLINE, 320, RULE_COL)

# Thin double rule under the overline
ru_w = 240
ru_x = (W - ru_w) // 2
draw.line([(ru_x, 390), (ru_x + ru_w, 390)], fill=RULE_COL, width=2)


# ---------------------------------------------------------------- title
y = 470
draw_centered("Build Your Own", F_TITLE, y, TITLE_COL)
y += 260
draw_centered("Operating System", F_TITLE, y, TITLE_COL)
y += 280

# Long horizontal rule under the title
hr_w = 900
hr_x = (W - hr_w) // 2
draw.line([(hr_x, y), (hr_x + hr_w, y)], fill=RULE_COL, width=3)
y += 50


# ---------------------------------------------------------------- subtitle
sub = "From boot sector to graphical desktop"
draw_centered(sub, F_SUB, y, SUB_COL)
y += 100


# ---------------------------------------------------------------- boot log
# A faux terminal block. Tells the reader, at a glance, what kind
# of book this is: kernels, drivers, subsystems coming up one by one.
log_lines = [
    ("[boot] ",  "console ready",                          LOG_OK),
    ("[boot] ",  "idt installed",                          LOG_OK),
    ("[pmm]  ",  "total = 255 MiB, free = 254 MiB",        LOG_BRIGHT),
    ("[heap] ",  "base=0x200000000 size=131072 B",         LOG_BRIGHT),
    ("[sched]",  " init, idle task id=0",                  LOG_BRIGHT),
    ("[ata]  ",  "primary slave ready",                    LOG_OK),
    ("[fb]   ",  "1024x768x32 @ 0xFD000000",               LOG_BRIGHT),
    ("[wm]   ",  "compositor up",                          LOG_OK),
]
log_y = 1640
line_h = 78
log_x = 230

# Faint terminal window outline
panel_pad = 60
panel_top = log_y - 100
panel_bottom = log_y + line_h * len(log_lines) + 60
panel_left = log_x - 50
panel_right = W - panel_left
# very subtle border
draw.rectangle(
    (panel_left, panel_top, panel_right, panel_bottom),
    outline=(50, 60, 78),
    width=2,
)
# title strip across the top of the terminal
strip_h = 56
draw.rectangle(
    (panel_left, panel_top, panel_right, panel_top + strip_h),
    fill=(28, 36, 50),
)
strip_label = "nexusos  ·  serial 115200 8N1"
draw.text(
    (panel_left + 30, panel_top + 12),
    strip_label,
    font=font("consola.ttf", 32),
    fill=(140, 150, 168),
)
# tiny mac-style dots, just for "this is a window" recognition
for i, col in enumerate(((220, 95, 90), (220, 190, 90), (120, 200, 130))):
    cx2 = panel_right - 40 - i * 36
    cy2 = panel_top + strip_h // 2
    draw.ellipse((cx2 - 11, cy2 - 11, cx2 + 11, cy2 + 11), fill=col)

# adjust log_y to sit below the strip
log_y = panel_top + strip_h + 50

for prefix, msg, color in log_lines:
    draw.text((log_x, log_y), prefix, font=F_LOG_BOLD, fill=LOG_DIM)
    px = log_x + text_w(prefix, F_LOG_BOLD)
    draw.text((px, log_y), msg, font=F_LOG, fill=color)
    log_y += line_h


# ---------------------------------------------------------------- author
ay = H - 360
draw_centered("Noah Parsons", F_AUTHOR, ay, AUTHOR_COL)
ay += 110
draw_centered(
    "Companion to the NexusOS reference kernel",
    F_AUTHOR_SUB, ay, SUB_COL,
)

# Bottom amber tick mark
tick_w = 80
draw.line(
    [((W - tick_w) // 2, H - 110), ((W + tick_w) // 2, H - 110)],
    fill=RULE_COL, width=3,
)


# ---------------------------------------------------------------- save
img.save(OUT, "PNG", optimize=True)
print(f"Wrote {OUT}  ({os.path.getsize(OUT)} bytes)")
