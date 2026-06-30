"""
build_book.py — render the NexusOS book manuscript to a PDF.

Reads `combined.md` (with the title block at the top, then preface and chapters)
and produces `BuildYourOwnOS.pdf`. The parser handles a deliberately small
subset of markdown — exactly what the manuscript uses:

  ::title / ::subtitle / ::author / ::end-title    — title page block
  # Chapter X — Title                              — chapter, page break
  # Preface / # Appendix A — ...                   — front/back matter
  ## Section                                       — section heading
  ### Subsection                                   — sub-section heading
  ```lang ... ```                                  — fenced code block
  *italic*, **bold**, `inline code`                — inline emphasis
  - bullet                                         — bullet list
  1. numbered                                      — numbered list
  > quote                                          — block quote / callout
  | a | b |                                        — table (header row first)
  *Listing N.M — caption.*                         — italic caption (rendered
                                                     as a small italic line)

Output is a real book layout: 6 x 9 trim, generous margins, serif body,
mono code, running headers / footers, proper chapter openings, and
forced page breaks between chapters.
"""

import argparse
import os
import re
import sys

from reportlab.lib.pagesizes import inch
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.enums import TA_LEFT, TA_CENTER, TA_JUSTIFY
from reportlab.lib import colors
from reportlab.lib.units import mm
from reportlab.platypus import (
    BaseDocTemplate, PageTemplate, Frame, Paragraph, Spacer, PageBreak,
    Preformatted, Table, TableStyle, KeepTogether, Flowable,
)
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfgen import canvas


HERE = os.path.dirname(os.path.abspath(__file__))

# Default paths for the full book. The sample build overrides these.
SRC = os.path.join(HERE, "combined.md")
OUT = os.path.join(HERE, "BuildYourOwnOS.pdf")

# Sample build settings, set via --sample on the command line.
SAMPLE_MODE = False
SAMPLE_SRC = os.path.join(HERE, "manuscript.md")   # already preface + ch1 + ch2
SAMPLE_OUT = os.path.join(HERE, "sample.pdf")
LEANPUB_URL = "leanpub.com/build-your-own-os"


# ---------------------------------------------------------------- fonts
# Use Georgia (Windows) for body and Consolas for code — both support the
# Unicode glyphs we need (arrows, math, ellipsis) that the PDF core fonts
# lack. Fall back to core Times/Courier silently if Georgia is unavailable.
def register_fonts():
    serif = "Times-Roman"
    serif_b = "Times-Bold"
    serif_i = "Times-Italic"
    serif_bi = "Times-BoldItalic"
    mono = "Courier"
    mono_b = "Courier-Bold"

    win_fonts = "C:/Windows/Fonts"
    # Cambria (.ttc, subfont 0) for the regular face — it has Unicode
    # arrows and math glyphs Georgia/Times lack. Bold/italic/bolditalic
    # are separate .ttf files on Windows.
    candidates = [
        ("Book-Serif",   "cambria.ttc",  "Book-Serif", 0),
        ("Book-Serif-B", "cambriab.ttf", "Book-Serif-B", None),
        ("Book-Serif-I", "cambriai.ttf", "Book-Serif-I", None),
        ("Book-Serif-BI","cambriaz.ttf", "Book-Serif-BI", None),
        ("Book-Mono",    "consola.ttf",  "Book-Mono", None),
        ("Book-Mono-B",  "consolab.ttf", "Book-Mono-B", None),
    ]
    registered = {}
    for name, fname, alias, subidx in candidates:
        path = os.path.join(win_fonts, fname)
        if os.path.exists(path):
            try:
                if subidx is not None:
                    pdfmetrics.registerFont(TTFont(alias, path,
                                                  subfontIndex=subidx))
                else:
                    pdfmetrics.registerFont(TTFont(alias, path))
                registered[name] = alias
            except Exception:
                pass

    if "Book-Serif" in registered and "Book-Serif-B" in registered \
       and "Book-Serif-I" in registered:
        serif    = registered["Book-Serif"]
        serif_b  = registered["Book-Serif-B"]
        serif_i  = registered["Book-Serif-I"]
        serif_bi = registered.get("Book-Serif-BI", serif_b)
        from reportlab.pdfbase.pdfmetrics import registerFontFamily
        registerFontFamily(serif,
                           normal=serif, bold=serif_b,
                           italic=serif_i, boldItalic=serif_bi)
    if "Book-Mono" in registered:
        mono = registered["Book-Mono"]
        mono_b = registered.get("Book-Mono-B", mono)
        from reportlab.pdfbase.pdfmetrics import registerFontFamily
        registerFontFamily(mono, normal=mono, bold=mono_b,
                           italic=mono, boldItalic=mono_b)

    return {
        "serif": serif, "serif_b": serif_b,
        "serif_i": serif_i, "serif_bi": serif_bi,
        "mono": mono, "mono_b": mono_b,
    }


FONTS = register_fonts()


# ---------------------------------------------------------------- page setup
PAGE_W, PAGE_H = 6.0 * inch, 9.0 * inch
# Symmetric 0.68" side margins: bumped slightly from 0.60" so a print-on-
# demand bind (typically eating ~0.1" on the inside edge) still leaves
# ~0.58" of live margin at the gutter. The remaining 4.64" usable width
# still fits the longest code lines in the manuscript at 7.6pt Consolas.
MARGIN_L = 0.68 * inch
MARGIN_R = 0.68 * inch
MARGIN_T = 0.85 * inch
MARGIN_B = 0.85 * inch


# ---------------------------------------------------------------- styles
def make_styles():
    base = getSampleStyleSheet()
    s = {}

    s["body"] = ParagraphStyle(
        name="body", parent=base["BodyText"],
        fontName=FONTS["serif"], fontSize=10.5, leading=14,
        alignment=TA_JUSTIFY, spaceBefore=0, spaceAfter=6,
        firstLineIndent=0,
    )
    s["body_first"] = ParagraphStyle(
        name="body_first", parent=s["body"],
        firstLineIndent=0,
    )
    s["caption"] = ParagraphStyle(
        name="caption", parent=s["body"],
        fontName=FONTS["serif_i"], fontSize=9.5, leading=12,
        spaceBefore=4, spaceAfter=2, alignment=TA_LEFT,
        textColor=colors.HexColor("#444444"),
    )
    s["chapter_num"] = ParagraphStyle(
        name="chapter_num", parent=base["Heading1"],
        fontName=FONTS["serif_i"], fontSize=11, leading=14,
        spaceBefore=0, spaceAfter=4, alignment=TA_LEFT,
        textColor=colors.HexColor("#888888"),
    )
    s["chapter_title"] = ParagraphStyle(
        name="chapter_title", parent=base["Heading1"],
        fontName=FONTS["serif_b"], fontSize=22, leading=26,
        spaceBefore=2, spaceAfter=24, alignment=TA_LEFT,
        textColor=colors.black,
    )
    s["frontmatter_title"] = ParagraphStyle(
        name="frontmatter_title", parent=base["Heading1"],
        fontName=FONTS["serif_b"], fontSize=22, leading=26,
        spaceBefore=0, spaceAfter=22, alignment=TA_LEFT,
    )
    s["h2"] = ParagraphStyle(
        name="h2", parent=base["Heading2"],
        fontName=FONTS["serif_b"], fontSize=13.5, leading=17,
        spaceBefore=14, spaceAfter=6, alignment=TA_LEFT,
        textColor=colors.black,
    )
    s["h3"] = ParagraphStyle(
        name="h3", parent=base["Heading3"],
        fontName=FONTS["serif_b"], fontSize=11, leading=14,
        spaceBefore=10, spaceAfter=4, alignment=TA_LEFT,
        textColor=colors.HexColor("#222222"),
    )
    s["code"] = ParagraphStyle(
        name="code", parent=base["Code"],
        fontName=FONTS["mono"], fontSize=7.6, leading=9.6,
        leftIndent=4, rightIndent=0,
        spaceBefore=4, spaceAfter=10,
        textColor=colors.HexColor("#1a1a1a"),
        backColor=colors.HexColor("#f5f5f0"),
        borderPadding=(5, 6, 5, 6),
    )
    s["quote"] = ParagraphStyle(
        name="quote", parent=s["body"],
        leftIndent=14, rightIndent=4,
        textColor=colors.HexColor("#333333"),
        fontName=FONTS["serif_i"],
        spaceBefore=6, spaceAfter=8,
    )
    s["bullet"] = ParagraphStyle(
        name="bullet", parent=s["body"],
        leftIndent=16, bulletIndent=4,
        spaceBefore=1, spaceAfter=3,
    )
    s["numbered"] = ParagraphStyle(
        name="numbered", parent=s["body"],
        leftIndent=20, bulletIndent=4,
        spaceBefore=1, spaceAfter=3,
    )
    s["ex_item"] = ParagraphStyle(
        name="ex_item", parent=s["body"],
        leftIndent=22, bulletIndent=4,
        spaceBefore=5, spaceAfter=3,
    )
    s["ex_hint"] = ParagraphStyle(
        name="ex_hint", parent=s["body"],
        fontName=FONTS["serif_i"], fontSize=10, leading=13,
        leftIndent=30, rightIndent=4,
        spaceBefore=1, spaceAfter=8,
        textColor=colors.HexColor("#444444"),
    )

    # title page
    s["title_main"] = ParagraphStyle(
        name="title_main", parent=base["Title"],
        fontName=FONTS["serif_b"], fontSize=34, leading=42,
        alignment=TA_CENTER, spaceBefore=0, spaceAfter=8,
    )
    s["title_sub"] = ParagraphStyle(
        name="title_sub", parent=base["Title"],
        fontName=FONTS["serif_i"], fontSize=15, leading=20,
        alignment=TA_CENTER, spaceBefore=0, spaceAfter=2,
        textColor=colors.HexColor("#333333"),
    )
    s["title_author"] = ParagraphStyle(
        name="title_author", parent=base["Title"],
        fontName=FONTS["serif"], fontSize=14, leading=18,
        alignment=TA_CENTER, spaceBefore=4, spaceAfter=0,
        textColor=colors.HexColor("#222222"),
    )
    s["title_sub_small"] = ParagraphStyle(
        name="title_sub_small", parent=base["Title"],
        fontName=FONTS["serif_i"], fontSize=10, leading=14,
        alignment=TA_CENTER, spaceBefore=0, spaceAfter=0,
        textColor=colors.HexColor("#666666"),
    )

    # FREE SAMPLE marker on the sample build's title page.
    s["sample_marker"] = ParagraphStyle(
        name="sample_marker", parent=base["Title"],
        fontName=FONTS["serif_b"], fontSize=15, leading=20,
        alignment=TA_CENTER, spaceBefore=0, spaceAfter=0,
        textColor=colors.HexColor("#c9a76f"),  # warm amber accent
    )

    return s


# ---------------------------------------------------------------- inline markup
def escape_xml(s):
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def fixup_glyphs(s):
    """Run AFTER inline markup conversion. Wraps arrows and special math
    glyphs in a non-italic font span so they render even when the
    surrounding prose is *italic* (Georgia Italic lacks U+2192 etc.)."""
    serif = FONTS["serif"]
    for ch in ("→", "←", "≥", "≤"):
        if ch in s:
            s = s.replace(ch, f'<font name="{serif}">{ch}</font>')
    return s


def inline_md_to_rl(text):
    """Convert a single line of markdown inline markup to ReportLab Paragraph
    inline HTML. Order matters: handle inline code first so its contents are
    not re-interpreted as emphasis or links."""
    # protect inline code spans by replacing them with placeholders
    code_spans = []

    def _code(m):
        code_spans.append(m.group(1))
        return f"\x00CODE{len(code_spans) - 1}\x00"

    text = re.sub(r"`([^`]+)`", _code, text)

    # Protect markdown links [text](url) BEFORE xml escape so the parens
    # in the URL aren't mistaken for prose. Render as a reportlab <link>
    # so the URL is clickable in the PDF; the visible text is the [text]
    # portion. Falls back to "text (url)" for paragraph parsers that lack
    # <link> support, but reportlab Paragraph supports it.
    link_spans = []

    def _link(m):
        link_spans.append((m.group(1), m.group(2)))
        return f"\x00LINK{len(link_spans) - 1}\x00"

    text = re.sub(r"\[([^\]]+)\]\(([^)\s]+)\)", _link, text)

    text = escape_xml(text)

    # bold then italic
    text = re.sub(r"\*\*([^*]+)\*\*", r"<b>\1</b>", text)
    text = re.sub(r"\*([^*]+)\*", r"<i>\1</i>", text)

    # restore inline code spans, escaped, in Courier
    def _restore(m):
        idx = int(m.group(1))
        body = escape_xml(code_spans[idx])
        return f'<font name="{FONTS["mono"]}" size="9">{body}</font>'

    text = re.sub(r"\x00CODE(\d+)\x00", _restore, text)

    # Restore link spans as reportlab <link> tags. The link text gets
    # the body markup (italic/bold inside link text is already applied
    # because the body went through the markup pass already), and the
    # url is xml-escaped so an ampersand doesn't break the parser.
    def _restore_link(m):
        idx = int(m.group(1))
        body, url = link_spans[idx]
        body_esc = escape_xml(body)
        url_esc = escape_xml(url)
        # Underline + accent colour so the link is visibly hyperlinked.
        return (f'<link href="{url_esc}"><font color="#1a4f7a">'
                f'<u>{body_esc}</u></font></link>')

    text = re.sub(r"\x00LINK(\d+)\x00", _restore_link, text)
    text = fixup_glyphs(text)
    return text


# ---------------------------------------------------------------- flowables
class HorizontalRule(Flowable):
    def __init__(self, width, thickness=0.4, color=colors.HexColor("#bbbbbb")):
        super().__init__()
        self.width = width
        self.thickness = thickness
        self.color = color

    def wrap(self, availW, availH):
        return self.width, self.thickness + 4

    def draw(self):
        c = self.canv
        c.setStrokeColor(self.color)
        c.setLineWidth(self.thickness)
        c.line(0, 2, self.width, 2)


# ---------------------------------------------------------------- parser
TOKEN_TITLE_BLOCK = "TITLE_BLOCK"
TOKEN_CHAPTER = "CHAPTER"            # # Chapter X — title or # Preface, etc.
TOKEN_H2 = "H2"
TOKEN_H3 = "H3"
TOKEN_CODE = "CODE"
TOKEN_CAPTION = "CAPTION"            # italic Listing line
TOKEN_PARA = "PARA"
TOKEN_BULLET = "BULLET"
TOKEN_NUMBERED = "NUMBERED"          # regular numbered list
TOKEN_EX_ITEM = "EX_ITEM"            # exercise numbered item
TOKEN_EX_HINT = "EX_HINT"
TOKEN_QUOTE = "QUOTE"
TOKEN_TABLE = "TABLE"
TOKEN_BLANK = "BLANK"
TOKEN_IMAGE = "IMAGE"


def parse_manuscript(text):
    """Tokenise the manuscript into a stream of (kind, payload) tuples."""
    tokens = []
    lines = text.splitlines()
    i = 0
    in_exercises = False  # exercise list formats hints specially

    # --- title block at top ---
    if lines and lines[0].strip() == "::title":
        title_lines = []
        sub_lines = []
        author_lines = []
        cur = None
        i = 0
        while i < len(lines):
            ln = lines[i].strip()
            if ln == "::title":
                cur = title_lines
            elif ln == "::subtitle":
                cur = sub_lines
            elif ln == "::author":
                cur = author_lines
            elif ln == "::end-title":
                i += 1
                break
            elif cur is not None and ln:
                cur.append(ln)
            i += 1
        tokens.append((TOKEN_TITLE_BLOCK, {
            "title": title_lines,
            "subtitle": sub_lines,
            "author": author_lines,
        }))

    while i < len(lines):
        ln = lines[i]
        s = ln.strip()

        # blank line
        if not s:
            tokens.append((TOKEN_BLANK, None))
            i += 1
            continue

        # fenced code block
        m = re.match(r"^```([A-Za-z0-9_+\-]*)\s*$", s)
        if m:
            lang = m.group(1) or "text"
            i += 1
            buf = []
            while i < len(lines):
                if re.match(r"^```\s*$", lines[i].rstrip()):
                    i += 1
                    break
                buf.append(lines[i])
                i += 1
            tokens.append((TOKEN_CODE, {"lang": lang, "lines": buf}))
            continue

        # chapter / front matter / appendix heading
        if s.startswith("# "):
            title = s[2:].strip()
            if title.lower().startswith("chapter "):
                # "Chapter X. title"  (accepts period, colon, em/en-dash)
                mm = re.match(r"^Chapter\s+(\d+)\s*[.:—-]\s*(.+)$", title)
                if mm:
                    tokens.append((TOKEN_CHAPTER, {
                        "kind": "chapter",
                        "num": mm.group(1),
                        "title": mm.group(2).strip(),
                    }))
                else:
                    tokens.append((TOKEN_CHAPTER, {
                        "kind": "chapter", "num": "", "title": title,
                    }))
            else:
                # preface / appendix / etc.
                tokens.append((TOKEN_CHAPTER, {
                    "kind": "front", "num": "", "title": title,
                }))
            in_exercises = False
            i += 1
            continue

        if s.startswith("## "):
            head = s[3:].strip()
            tokens.append((TOKEN_H2, head))
            in_exercises = head.lower().startswith("exercise")
            i += 1
            continue

        if s.startswith("### "):
            tokens.append((TOKEN_H3, s[4:].strip()))
            i += 1
            continue

        # block quote / callout
        if s.startswith("> "):
            buf = []
            while i < len(lines) and lines[i].strip().startswith(">"):
                line = lines[i].strip()[1:].lstrip()
                buf.append(line)
                i += 1
            tokens.append((TOKEN_QUOTE, " ".join(buf)))
            continue

        # table — any line that starts with "|" and ends with "|"
        if s.startswith("|") and s.endswith("|"):
            rows = []
            while i < len(lines):
                rs = lines[i].strip()
                if not (rs.startswith("|") and rs.endswith("|")):
                    break
                # skip separator row e.g. |---|---|
                if re.match(r"^\|[\s\-:|]+\|$", rs):
                    i += 1
                    continue
                cells = [c.strip() for c in rs.strip("|").split("|")]
                rows.append(cells)
                i += 1
            if rows:
                tokens.append((TOKEN_TABLE, rows))
            continue

        # caption line: "*Listing N.M. caption.*"  italic on a line alone.
        # Accept period, colon, em-dash, or en-dash after the listing number.
        if re.match(r"^\*Listing\s+[0-9.]+\s*[.:—-]", s) and s.endswith("*"):
            tokens.append((TOKEN_CAPTION, s[1:-1].strip()))
            i += 1
            continue

        # Standalone image line: ![alt](path) or ![alt](path "caption")
        m = re.match(r'^!\[([^\]]*)\]\(([^)\s]+)(?:\s+"([^"]+)")?\)$', s)
        if m:
            tokens.append((TOKEN_IMAGE, {
                "alt": m.group(1),
                "path": m.group(2),
                "caption": m.group(3) or "",
            }))
            i += 1
            continue

        # numbered list item, with indented continuation lines
        m = re.match(r"^(\d+)\.\s+(.*)$", s)
        if m:
            content = m.group(2)
            i += 1
            # gather wrapped lines (indented or until blank)
            while i < len(lines) and lines[i].startswith("   ") and lines[i].strip():
                # a hint line starts with "   *Hint:*"
                stripped = lines[i].strip()
                if stripped.startswith("*Hint:*"):
                    # flush current
                    if in_exercises:
                        tokens.append((TOKEN_EX_ITEM, content))
                    else:
                        tokens.append((TOKEN_NUMBERED, content))
                    tokens.append((TOKEN_EX_HINT, stripped[len("*Hint:*"):].strip()))
                    content = None
                    i += 1
                    # consume any continuation of the hint
                    while i < len(lines) and lines[i].startswith("   ") and lines[i].strip() and not lines[i].strip().startswith("*Hint:*"):
                        # extend the last EX_HINT
                        prev_kind, prev_payload = tokens[-1]
                        tokens[-1] = (prev_kind, prev_payload + " " + lines[i].strip())
                        i += 1
                    break
                else:
                    content += " " + stripped
                    i += 1
            if content is not None:
                if in_exercises:
                    tokens.append((TOKEN_EX_ITEM, content))
                else:
                    tokens.append((TOKEN_NUMBERED, content))
            continue

        # bullet
        if s.startswith("- "):
            content = s[2:]
            i += 1
            while i < len(lines) and lines[i].startswith("  ") and lines[i].strip():
                content += " " + lines[i].strip()
                i += 1
            tokens.append((TOKEN_BULLET, content))
            continue

        # paragraph — gather until blank line, code fence, heading, list, etc.
        buf = [s]
        i += 1
        while i < len(lines):
            nxt = lines[i]
            ns = nxt.strip()
            if not ns:
                break
            if ns.startswith(("#", "```", "- ", "> ", "|")):
                break
            if re.match(r"^\d+\.\s+", ns):
                break
            buf.append(ns)
            i += 1
        tokens.append((TOKEN_PARA, " ".join(buf)))

    return tokens


# ---------------------------------------------------------------- canvas
class BookCanvas(canvas.Canvas):
    """Adds the running header + footer with chapter title and page number."""

    def __init__(self, *args, page_meta=None, **kw):
        super().__init__(*args, **kw)
        self._saved = []
        self.page_meta = page_meta or {}

    def showPage(self):
        self._saved.append(dict(self.__dict__))
        self._startPage()

    def save(self):
        total = len(self._saved)
        for n, state in enumerate(self._saved, start=1):
            self.__dict__.update(state)
            self._draw_chrome(n, total)
            super().showPage()
        super().save()

    def _draw_chrome(self, page_num, total):
        meta = self.page_meta.get(page_num, {})
        kind = meta.get("kind", "body")
        title = meta.get("title", "")
        # Skip header/footer on the title page (page 1) and chapter-open pages.
        if kind == "title":
            return

        # Header rule
        self.setStrokeColor(colors.HexColor("#cccccc"))
        self.setLineWidth(0.3)
        if kind != "chapter_open":
            self.line(MARGIN_L, PAGE_H - MARGIN_T + 16,
                      PAGE_W - MARGIN_R, PAGE_H - MARGIN_T + 16)
            # Header text: chapter title (centred)
            self.setFont("Times-Italic", 9)
            self.setFillColor(colors.HexColor("#666666"))
            self.drawCentredString(PAGE_W / 2.0, PAGE_H - MARGIN_T + 22, title)

        # Footer: page number centred
        self.setFont("Times-Roman", 9)
        self.setFillColor(colors.HexColor("#444444"))
        # Skip page number "1" since title page is not numbered visually
        self.drawCentredString(PAGE_W / 2.0, MARGIN_B - 28, str(page_num))


# ---------------------------------------------------------------- render
class BookBuilder:
    def __init__(self):
        self.styles = make_styles()
        self.story = []
        self.page_meta = {}  # page_num -> {kind, title}
        self.current_chapter_title = ""
        self.page_counter = 0

    def add(self, flowable):
        self.story.append(flowable)

    def add_title_page(self, payload):
        # Push generous top space, then centred title block
        if SAMPLE_MODE:
            # Sample title page: a small amber pre-title strip identifies
            # this as the free sample without crowding the main title.
            # Use &#160; (non-breaking space) to fake letter spacing; the
            # Paragraph parser otherwise collapses multiple spaces.
            self.add(Spacer(1, 1.1 * inch))
            nbsp = "&#160;"
            spaced = (nbsp * 2).join(list("FREE")) + (nbsp * 4) + \
                     (nbsp * 2).join(list("SAMPLE"))
            self.add(Paragraph(spaced, self.styles["sample_marker"]))
            self.add(Spacer(1, 0.25 * inch))
        else:
            self.add(Spacer(1, 1.5 * inch))
        for ln in payload["title"]:
            self.add(Paragraph(escape_xml(ln), self.styles["title_main"]))
        self.add(Spacer(1, 0.4 * inch))
        for ln in payload["subtitle"]:
            self.add(Paragraph(escape_xml(ln), self.styles["title_sub"]))
        self.add(Spacer(1, 1.8 * inch))
        for ln in payload["author"]:
            self.add(Paragraph(escape_xml(ln), self.styles["title_author"]))
        self.add(Spacer(1, 0.15 * inch))
        self.add(Paragraph(
            "<i>Companion to the NexusOS reference kernel</i>",
            self.styles["title_sub_small"]))
        if SAMPLE_MODE:
            self.add(Spacer(1, 0.4 * inch))
            self.add(Paragraph(
                f'<i>Sample: Preface + Chapter 1 + Chapter 2.<br/>'
                f'Full book at {LEANPUB_URL}</i>',
                self.styles["title_sub_small"]))
        self.add(PageBreak())

    def add_chapter(self, payload):
        # New page for every chapter / front matter / appendix
        self.add(PageBreak())
        title = payload["title"]
        kind = payload["kind"]
        if kind == "chapter":
            # "Chapter N" overline, then big title
            self.add(Spacer(1, 0.3 * inch))
            self.add(Paragraph(
                f"CHAPTER {payload['num']}", self.styles["chapter_num"]))
            self.add(HorizontalRule(PAGE_W - MARGIN_L - MARGIN_R,
                                    thickness=0.5,
                                    color=colors.HexColor("#888888")))
            self.add(Spacer(1, 4))
            self.add(Paragraph(escape_xml(title), self.styles["chapter_title"]))
            self.current_chapter_title = f"Ch. {payload['num']} · {title}"
            self.current_plain_title = title
        else:
            self.add(Spacer(1, 0.3 * inch))
            self.add(Paragraph(escape_xml(title),
                               self.styles["frontmatter_title"]))
            self.current_chapter_title = title
            self.current_plain_title = title
        # Mark this open page so we don't paint the running header on it
        # (we'll detect at draw time via a sentinel in page_meta).
        self.add(_ChapterOpenMarker(self.current_chapter_title,
                                    self.current_plain_title))

    def add_h2(self, text):
        self.add(Paragraph(inline_md_to_rl(text), self.styles["h2"]))

    def add_h3(self, text):
        self.add(Paragraph(inline_md_to_rl(text), self.styles["h3"]))

    def add_para(self, text):
        self.add(Paragraph(inline_md_to_rl(text), self.styles["body"]))

    def add_quote(self, text):
        self.add(Paragraph(inline_md_to_rl(text), self.styles["quote"]))

    def add_bullet(self, text):
        self.add(Paragraph(inline_md_to_rl(text),
                           self.styles["bullet"], bulletText="•"))

    def add_numbered(self, text, n):
        self.add(Paragraph(inline_md_to_rl(text),
                           self.styles["numbered"], bulletText=f"{n}."))

    def add_ex_item(self, text, n):
        self.add(Paragraph(inline_md_to_rl(text),
                           self.styles["ex_item"], bulletText=f"{n}."))

    def add_ex_hint(self, text):
        text = "<i>Hint.</i> " + inline_md_to_rl(text)
        self.add(Paragraph(text, self.styles["ex_hint"]))

    def add_caption(self, text):
        self.add(Paragraph(inline_md_to_rl(text), self.styles["caption"]))

    def add_code(self, lines):
        # Trim trailing blank lines
        while lines and not lines[-1].strip():
            lines.pop()
        text = "\n".join(lines)
        self.add(Preformatted(text, self.styles["code"]))

    def add_table(self, rows):
        # First row is header (typical for our manuscript tables)
        rendered = []
        for ri, row in enumerate(rows):
            rendered.append([Paragraph(inline_md_to_rl(c),
                                       self.styles["body"]) for c in row])
        # Compute column widths: distribute total width across columns,
        # but give a bit more to the last column for descriptions.
        usable = PAGE_W - MARGIN_L - MARGIN_R
        n = len(rows[0])
        if n == 2:
            widths = [usable * 0.30, usable * 0.70]
        else:
            widths = [usable / n] * n
        t = Table(rendered, colWidths=widths, repeatRows=1)
        t.setStyle(TableStyle([
            ("FONT", (0, 0), (-1, 0), FONTS["serif_b"]),
            ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#ececec")),
            ("BOX", (0, 0), (-1, -1), 0.4, colors.HexColor("#888888")),
            ("INNERGRID", (0, 0), (-1, -1), 0.3, colors.HexColor("#cccccc")),
            ("VALIGN", (0, 0), (-1, -1), "TOP"),
            ("LEFTPADDING", (0, 0), (-1, -1), 5),
            ("RIGHTPADDING", (0, 0), (-1, -1), 5),
            ("TOPPADDING", (0, 0), (-1, -1), 3),
            ("BOTTOMPADDING", (0, 0), (-1, -1), 3),
        ]))
        self.add(t)
        self.add(Spacer(1, 6))

    def add_image(self, payload):
        from reportlab.platypus import Image
        path = payload["path"]
        caption = payload["caption"]
        # Resolve relative paths against the manuscript directory.
        if not os.path.isabs(path):
            path = os.path.normpath(os.path.join(os.path.dirname(SRC), path))
        if not os.path.exists(path):
            # Fail soft: render an italic placeholder so layout doesn't break.
            self.add(Paragraph(
                f"<i>[image not found: {escape_xml(path)}]</i>",
                self.styles["caption"]))
            return
        usable = PAGE_W - MARGIN_L - MARGIN_R
        try:
            img = Image(path)
            # Scale so the image's pixel width = usable width, preserving ratio.
            w, h = img.imageWidth, img.imageHeight
            scale = usable / float(w)
            img.drawWidth = usable
            img.drawHeight = h * scale
            # Cap height at 75% of frame so caption stays on the same page.
            max_h = (PAGE_H - MARGIN_T - MARGIN_B) * 0.7
            if img.drawHeight > max_h:
                img.drawWidth *= max_h / img.drawHeight
                img.drawHeight = max_h
        except Exception as e:
            self.add(Paragraph(
                f"<i>[image error: {escape_xml(str(e))}]</i>",
                self.styles["caption"]))
            return
        self.add(Spacer(1, 4))
        self.add(img)
        if caption:
            self.add(Paragraph(inline_md_to_rl(caption),
                               self.styles["caption"]))
        self.add(Spacer(1, 6))


# Sentinel flowable used so the canvas knows a given page is a chapter-open
# page (no running header). It draws nothing visible; it just contributes
# its title to page_meta when laid out.
class _ChapterOpenMarker(Flowable):
    def __init__(self, title, plain_title=None):
        super().__init__()
        self.title = title  # running-header form, e.g. "Ch. 1 · Title"
        self.plain_title = plain_title or title  # for TOC lookup

    def wrap(self, w, h):
        return 0, 0

    def draw(self):
        # We can't easily know the page number from here without state on
        # the canvas. We set an attribute on the canvas; the draw chrome
        # uses it to mark the *current* page as chapter_open.
        c = self.canv
        if not hasattr(c, "_pending_chapter_marks"):
            c._pending_chapter_marks = []
        c._pending_chapter_marks.append((c.getPageNumber(), self.title))


# ---------------------------------------------------------------- main
def render_tokens_to_story(tokens):
    """Build a fresh BookBuilder.story from a token stream. Called twice
    (once per layout pass) so each pass gets its own flowable objects."""
    builder = BookBuilder()
    ex_num = 0
    in_ex = False
    list_num = 0
    for kind, payload in tokens:
        if kind == TOKEN_TITLE_BLOCK:
            title_block = payload
            builder.add_title_page(payload)
            current_title = "Build Your Own Operating System"
        elif kind == TOKEN_CHAPTER:
            builder.add_chapter(payload)
            if payload["kind"] == "chapter":
                current_title = f"Ch. {payload['num']} · {payload['title']}"
            else:
                current_title = payload["title"]
            in_ex = False
            ex_num = 0
            list_num = 0
        elif kind == TOKEN_H2:
            builder.add_h2(payload)
            if payload.lower().startswith("exercise"):
                in_ex = True
                ex_num = 0
            else:
                in_ex = False
            list_num = 0
        elif kind == TOKEN_H3:
            builder.add_h3(payload)
        elif kind == TOKEN_PARA:
            builder.add_para(payload)
        elif kind == TOKEN_QUOTE:
            builder.add_quote(payload)
        elif kind == TOKEN_BULLET:
            builder.add_bullet(payload)
        elif kind == TOKEN_NUMBERED:
            list_num += 1
            builder.add_numbered(payload, list_num)
        elif kind == TOKEN_EX_ITEM:
            ex_num += 1
            builder.add_ex_item(payload, ex_num)
        elif kind == TOKEN_EX_HINT:
            builder.add_ex_hint(payload)
        elif kind == TOKEN_CAPTION:
            builder.add_caption(payload)
        elif kind == TOKEN_CODE:
            builder.add_code(payload["lines"])
        elif kind == TOKEN_TABLE:
            builder.add_table(payload)
        elif kind == TOKEN_IMAGE:
            builder.add_image(payload)
        elif kind == TOKEN_BLANK:
            # paragraphs already carry their own spacing
            pass
    return builder.story


def build():
    with open(SRC, "r", encoding="utf-8") as f:
        text = f.read()
    tokens = parse_manuscript(text)

    # Sample build: only the title block + Preface + Chapter 1 + Chapter 2.
    # The source file is already manuscript.md (which contains exactly those
    # three sections), so the token stream is naturally limited; we just
    # need to swap in sample-specific cover/copyright/closing pages and
    # skip the full TOC (the sample is too short to need one).
    if SAMPLE_MODE:
        tokens = _inject_sample_chrome(tokens)

    # We need to know, for each page: the chapter title (running header)
    # and whether this is a chapter-open page (no header). ReportLab's
    # onPage callback fires BEFORE the page's flowables draw, so we cannot
    # learn that from a flowable on the page itself. Solution: two passes.
    #
    # Pass 1: lay out, capture per-page kind + chapter title, and capture
    # each chapter's opening page number for the TOC.
    # Pass 2: lay out again, draw the chrome and render a real TOC using
    # the page numbers captured in pass 1.

    page_info = {}  # page_no -> {"title": str, "kind": "title"|"chapter_open"|"body"|"toc"}
    toc_entries = []  # list of {"label": str, "title": str, "page": int}
    state = {"current": "Build Your Own Operating System",
             "page_info": page_info,
             "kind": "title",
             "toc_entries": toc_entries}

    # Pre-compute the chapter list from tokens so the pass-1 TOC placeholder
    # has the right number of rows (and therefore the right height).
    chapter_list = []
    for kind, payload in tokens:
        if kind == TOKEN_CHAPTER:
            if payload["kind"] == "chapter":
                label = f"Chapter {payload['num']}"
            else:
                label = ""
            chapter_list.append({"label": label, "title": payload["title"]})

    # --- Pass 1: dry layout with placeholder TOC ---------------------------
    raw_story1 = render_tokens_to_story(tokens)
    if SAMPLE_MODE:
        # Sample is short; no TOC. Just markers for chrome bookkeeping.
        pass1_story = _wrap_without_toc(raw_story1, page_info, state,
                                        recording=True)
    else:
        pass1_story = _wrap_with_toc(raw_story1, chapter_list, page_info,
                                     state, toc_placeholder=True)

    def pass1_decorator(canv, _doc):
        page_no = canv.getPageNumber()
        # Record what we know at start-of-page; the flipper may upgrade
        # this to chapter_open mid-page.
        page_info.setdefault(page_no, {
            "title": state["current"],
            "kind": state.get("kind", "body"),
        })
        if state.get("kind") == "chapter_open":
            state["kind"] = "body"

    doc1 = BaseDocTemplate(
        os.path.join(os.path.dirname(OUT), "_pass1.pdf"),
        pagesize=(PAGE_W, PAGE_H),
        leftMargin=MARGIN_L, rightMargin=MARGIN_R,
        topMargin=MARGIN_T, bottomMargin=MARGIN_B,
    )
    frame = Frame(MARGIN_L, MARGIN_B,
                  PAGE_W - MARGIN_L - MARGIN_R,
                  PAGE_H - MARGIN_T - MARGIN_B,
                  id="normal", leftPadding=0, rightPadding=0,
                  topPadding=0, bottomPadding=0)
    doc1.addPageTemplates([PageTemplate(id="p1", frames=[frame],
                                        onPage=pass1_decorator)])

    doc1.build(pass1_story)
    try:
        os.remove(os.path.join(os.path.dirname(OUT), "_pass1.pdf"))
    except OSError:
        pass

    # --- Pass 2: real layout, chrome driven by page_info -------------------
    def pass2_decorator(canv, _doc):
        page_no = canv.getPageNumber()
        info = page_info.get(page_no, {"title": "", "kind": "body"})
        title = info["title"]
        kind = info["kind"]

        if kind == "title" or page_no == 1 or kind == "toc":
            return
        if kind == "chapter_open":
            canv.saveState()
            canv.setFont(FONTS["serif"], 9)
            canv.setFillColor(colors.HexColor("#444444"))
            canv.drawCentredString(PAGE_W / 2.0, MARGIN_B - 22, str(page_no))
            canv.restoreState()
            return

        canv.saveState()
        canv.setStrokeColor(colors.HexColor("#cccccc"))
        canv.setLineWidth(0.3)
        canv.line(MARGIN_L, PAGE_H - MARGIN_T + 16,
                  PAGE_W - MARGIN_R, PAGE_H - MARGIN_T + 16)
        canv.setFont(FONTS["serif_i"], 9)
        canv.setFillColor(colors.HexColor("#666666"))
        canv.drawCentredString(PAGE_W / 2.0, PAGE_H - MARGIN_T + 22, title)
        canv.setFont(FONTS["serif"], 9)
        canv.setFillColor(colors.HexColor("#444444"))
        canv.drawCentredString(PAGE_W / 2.0, MARGIN_B - 22, str(page_no))
        canv.restoreState()

    doc2 = BaseDocTemplate(
        OUT,
        pagesize=(PAGE_W, PAGE_H),
        leftMargin=MARGIN_L, rightMargin=MARGIN_R,
        topMargin=MARGIN_T, bottomMargin=MARGIN_B,
        title="Build Your Own Operating System",
        author="Noah Parsons",
        subject="A hands-on x86-64 kernel companion to NexusOS, from boot sector to graphical desktop",
        keywords=("operating systems, kernel development, x86-64, bare-metal, "
                  "bootloader, BIOS, C programming, assembly, QEMU, NexusOS"),
        creator="ReportLab (custom build_book.py pipeline)",
    )
    frame2 = Frame(MARGIN_L, MARGIN_B,
                   PAGE_W - MARGIN_L - MARGIN_R,
                   PAGE_H - MARGIN_T - MARGIN_B,
                   id="normal", leftPadding=0, rightPadding=0,
                   topPadding=0, bottomPadding=0)
    doc2.addPageTemplates([PageTemplate(id="p2", frames=[frame2],
                                        onPage=pass2_decorator)])

    # Pass 2: rebuild a fresh story (flowables can't be reused across
    # build()s — reportlab caches wrap state on Paragraphs).
    raw_story2 = render_tokens_to_story(tokens)
    if SAMPLE_MODE:
        pass2_story = _wrap_without_toc(raw_story2, page_info, state,
                                        recording=False)
    else:
        pass2_story = _wrap_with_toc(raw_story2, chapter_list, page_info, state,
                                     toc_placeholder=False)
    doc2.build(pass2_story)


def _wrap_without_toc(raw_story, page_info, state, recording):
    """Sample-build variant of _wrap_with_toc: no TOC inserted, but the
    chapter-open flippers still need to be wired so the running header
    and chapter-open page detection work. recording=True is pass 1."""
    final = []
    chapter_index = [0]
    for f in raw_story:
        if isinstance(f, _ChapterOpenMarker):
            if recording:
                final.append(_TOCRecordingFlipper(
                    f.title, f.plain_title, page_info, state, chapter_index))
            else:
                final.append(_TitleFlipper(f.title, state))
        else:
            final.append(f)
    return final


def _inject_sample_chrome(tokens):
    """Add sample-specific front and back matter into the token stream.

    The manuscript file (which we read in sample mode) already starts with
    the title block and ends with Chapter 2. We add:
      - a 'FREE SAMPLE' marker as a title-block subtitle line addition
      - a copyright/colophon page after the title page
      - an 'End of free sample' page after Chapter 2 with the Leanpub link
    """
    out = []
    # Title block: augment the subtitle with a FREE SAMPLE marker line.
    title_done = False
    for kind, payload in tokens:
        if kind == TOKEN_TITLE_BLOCK and not title_done:
            # Keep the title block as-is — the FREE SAMPLE indicator is
            # rendered in add_title_page when SAMPLE_MODE is on.
            out.append((kind, payload))
            # Right after the title block, insert a synthesised "Colophon"
            # front-matter chapter that acts as the copyright/intro page.
            out.append((TOKEN_CHAPTER, {
                "kind": "front",
                "num": "",
                "title": "About this sample",
            }))
            for tok in _sample_colophon_tokens():
                out.append(tok)
            title_done = True
        else:
            out.append((kind, payload))

    # Closing "end of sample" front-matter chapter at the end.
    out.append((TOKEN_CHAPTER, {
        "kind": "front",
        "num": "",
        "title": "End of free sample",
    }))
    for tok in _sample_closing_tokens():
        out.append(tok)
    return out


def _sample_colophon_tokens():
    """Tokens for the sample's colophon / copyright page."""
    paras = [
        "This is a free sample of *Build Your Own Operating System*, a "
        "hands-on book that takes you from a 512-byte BIOS boot sector to "
        "a graphical desktop running on real hardware, paired with "
        "**NexusOS**, a roughly three-thousand-line x86-64 reference "
        "kernel you can clone, build, and modify.",

        "What you have here is the front of the book: the Preface, "
        "Chapter 1 (what an operating system is and what we will build), "
        "and Chapter 2 (your toolchain and the first booting OS, a "
        "512-byte program that prints `OK` on a virtual machine). "
        "Chapter 2 ends at the natural cliffhanger: a working boot sector, "
        "and the question of why exactly `[BITS 16]`, `0x7C00`, and `0xAA55` "
        "are what they are. Chapter 3 answers that, and the remaining ten "
        "chapters build the rest of the kernel on top.",

        f"If this front matter pulls you in, the full book is at "
        f"[{LEANPUB_URL}](https://{LEANPUB_URL}). NexusOS itself is on "
        "GitHub at [github.com/GuiloScion/NexusOS](https://github.com/GuiloScion/NexusOS), "
        "MIT-licensed, with the source pinned to the `v1.0-book` tag.",

        "Copyright © 2026 Noah Parsons. All rights reserved on the prose; "
        "the NexusOS source code referenced throughout is MIT-licensed. "
        "Please do not redistribute this sample as if it were the whole "
        "book; do share the Leanpub link with anyone who might want it.",
    ]
    return [(TOKEN_PARA, p) for p in paras]


def _sample_closing_tokens():
    """Tokens for the 'end of sample, buy the book' closing page."""
    paras = [
        "**This is the end of the free sample.** You have the booting "
        "boot sector from Chapter 2. The rest of the book builds the OS "
        "on top of it:",

        "Chapter 3 explains *why* the BIOS hands control to your code "
        "at `0x7C00` and what tools it leaves you in real mode. "
        "Chapter 4 walks the CPU from 16-bit real mode through 32-bit "
        "protected mode into 64-bit long mode, with the actual page-table "
        "ritual the kernel uses. Chapter 5 lands you in C with two output "
        "channels (VGA text and the COM1 serial port) and a linker script. "
        "Chapter 6 builds the IDT, the PIC, the timer, and the keyboard, "
        "and turns the kernel's exceptions into readable panic dumps. "
        "Chapter 7 is the memory manager: PMM, VMM, heap, all in one "
        "self-contained stack. Chapter 8 adds a preemptive scheduler with "
        "mutexes and semaphores. Chapter 9 talks to a disk. Chapter 10 "
        "draws pixels. Chapter 11 composites windows. Chapter 12 is the "
        "hard, honest chapter on real hardware. Chapter 13 maps what to "
        "build after.",

        f"The full book is at [{LEANPUB_URL}](https://{LEANPUB_URL}). "
        "It is roughly one hundred pages of focused practitioner content, "
        "thirteen chapters of working subsystems, a glossary, a full-source "
        "appendix, and the back-of-the-book references that make OS "
        "development tractable.",

        "Thank you for reading the sample. If it landed for you, "
        "the most useful thing you can do beyond buying the book is to "
        "actually build one of NexusOS's subsystems and send a pull request.",
    ]
    return [(TOKEN_PARA, p) for p in paras]


def _wrap_with_toc(raw_story, chapter_list, page_info, state,
                   toc_placeholder):
    """Insert a Contents page after the title page, mark its kind in page_info,
    and replace _ChapterOpenMarker with the right flipper for the pass.

    Pass 1: toc_placeholder=True, flippers also record page_info into the
    shared state's toc_entries via _TOCRecordingFlipper. The TOC content
    itself uses dotted-line placeholders (same row count and height as the
    real TOC).
    Pass 2: toc_placeholder=False, flippers are simple _TitleFlipper (no
    recording), and the TOC content uses real page numbers from
    state["toc_entries"]."""

    out = []
    title_page_done = False
    for f in raw_story:
        out.append(f)
        # Insert TOC after the title page's PageBreak. The title page emits
        # a PageBreak as its last flowable.
        if (not title_page_done
                and isinstance(f, PageBreak)):
            out.append(_TOCStartMarker(state))
            out.append(_TOCFlowable(chapter_list, state, toc_placeholder))
            out.append(PageBreak())
            title_page_done = True

    # Replace chapter-open markers with the right flipper for the pass.
    final = []
    chapter_index = [0]  # mutable counter
    for f in out:
        if isinstance(f, _ChapterOpenMarker):
            if toc_placeholder:
                final.append(_TOCRecordingFlipper(
                    f.title, f.plain_title, page_info, state, chapter_index))
            else:
                final.append(_TitleFlipper(f.title, state))
        else:
            final.append(f)
    return final


class _TOCStartMarker(Flowable):
    """Zero-size flowable that marks the current page as the Contents page
    in page_info, so the running header is suppressed for it."""
    def __init__(self, state):
        super().__init__()
        self.state = state

    def wrap(self, w, h):
        return 0, 0

    def draw(self):
        page_no = self.canv.getPageNumber()
        self.state["page_info"][page_no] = {
            "title": "Contents", "kind": "toc"
        }


class _TOCFlowable(Flowable):
    """Renders the table of contents. Both passes produce the same height
    (same row count, same fonts, same spacing). Pass 1 fills page numbers
    with dotted placeholders; pass 2 uses real numbers from state."""

    ROW_LEAD = 16   # leading per row, pt

    def __init__(self, chapter_list, state, placeholder):
        super().__init__()
        self.chapter_list = chapter_list
        self.state = state
        self.placeholder = placeholder

    def wrap(self, avail_w, avail_h):
        self.width = avail_w
        # heading height + spacing + rows
        self.heading_h = 36
        self.rows_h = len(self.chapter_list) * self.ROW_LEAD
        self.height = self.heading_h + self.rows_h + 8
        return self.width, self.height

    def draw(self):
        c = self.canv
        # Heading
        c.setFont(FONTS["serif_b"], 22)
        c.setFillColor(colors.black)
        c.drawString(0, self.height - 26, "Contents")
        # Underline rule
        c.setStrokeColor(colors.HexColor("#888888"))
        c.setLineWidth(0.5)
        c.line(0, self.height - 32, self.width, self.height - 32)

        # Lookup page numbers (pass 2). In pass 1 there is none yet.
        entries = self.state.get("toc_entries", [])
        page_lookup = {e["title"]: e["page"] for e in entries}

        # Rows
        y = self.height - 36 - self.ROW_LEAD
        # Reserve enough at the right for a 3-digit page number.
        max_title_w = self.width - 22
        for ch in self.chapter_list:
            label = ch["label"]
            title = ch["title"]

            # left text: "Chapter N. Title" or just "Title" for front/back matter
            if label:
                left = f"{label}. {title}"
            else:
                left = title

            # Clip with ellipsis if it would otherwise collide with the page no.
            c.setFont(FONTS["serif"], 10.5)
            if c.stringWidth(left, FONTS["serif"], 10.5) > max_title_w:
                while (left
                       and c.stringWidth(left + "…", FONTS["serif"], 10.5)
                           > max_title_w):
                    left = left[:-1]
                left = left.rstrip(" ,.;") + "…"

            c.setFillColor(colors.HexColor("#222222"))
            c.drawString(0, y, left)

            # right text: page number (real or placeholder)
            if self.placeholder:
                right = "·"
            else:
                right = str(page_lookup.get(title, ""))
            c.setFont(FONTS["serif"], 10.5)
            c.drawRightString(self.width, y, right)

            y -= self.ROW_LEAD


class _TitleFlipper(Flowable):
    """A zero-size flowable that, when its draw is called, updates the
    shared state dict used by the page decorator: marks the *current*
    page as a chapter-open page and sets the new chapter title."""
    def __init__(self, title, state):
        super().__init__()
        self.title = title
        self.state = state

    def wrap(self, w, h):
        return 0, 0

    def draw(self):
        self.state["current"] = self.title
        self.state["kind"] = "chapter_open"


class _RecordingFlipper(Flowable):
    """Pass-1 variant: records, for the page on which it actually draws,
    that the page is a chapter-open page with the given title. Also
    updates shared state so subsequent pages inherit the new title."""
    def __init__(self, title, page_info, state):
        super().__init__()
        self.title = title
        self.page_info = page_info
        self.state = state

    def wrap(self, w, h):
        return 0, 0

    def draw(self):
        page_no = self.canv.getPageNumber()
        # This page is a chapter-open page (override what onPage put in).
        self.page_info[page_no] = {"title": self.title, "kind": "chapter_open"}
        # Future pages should run with this title (body, with header).
        self.state["current"] = self.title
        self.state["kind"] = "body"


class _TOCRecordingFlipper(Flowable):
    """Pass-1 variant used when a TOC is being built: in addition to the
    normal recording behaviour, appends an entry to state['toc_entries']
    so pass 2 can render real page numbers."""
    def __init__(self, title, plain_title, page_info, state, chapter_index):
        super().__init__()
        self.title = title
        self.plain_title = plain_title
        self.page_info = page_info
        self.state = state
        self.chapter_index = chapter_index

    def wrap(self, w, h):
        return 0, 0

    def draw(self):
        page_no = self.canv.getPageNumber()
        self.page_info[page_no] = {"title": self.title, "kind": "chapter_open"}
        self.state["current"] = self.title
        self.state["kind"] = "body"
        self.state["toc_entries"].append({
            "title": self.plain_title,
            "page": page_no,
        })
        self.chapter_index[0] += 1


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description=("Build BuildYourOwnOS.pdf from the manuscript markdown. "
                     "Pass --sample to instead build sample.pdf (Preface + "
                     "Chapter 1 + Chapter 2 only, with sample-specific "
                     "title-page indicator, colophon, and closing page)."),
    )
    parser.add_argument(
        "--sample", action="store_true",
        help="Build the free sample PDF (sample.pdf) instead of the full book.",
    )
    args = parser.parse_args()

    if args.sample:
        # Module-level globals so build() and its helpers see the override.
        globals()["SAMPLE_MODE"] = True
        globals()["SRC"] = SAMPLE_SRC
        globals()["OUT"] = SAMPLE_OUT

    build()
    print(f"Built {OUT}")
