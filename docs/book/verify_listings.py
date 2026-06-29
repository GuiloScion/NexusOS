"""
verify_listings.py — sanity-check every `Listing X.Y` caption in the manuscript
against the actual file contents at a given git tag (default: v1.0-book).

For each caption of the form
    *Listing 3.1. `bootloader/boot.asm`, lines 12-19: setting up...*
this script:

  1. extracts the file path and line range(s)
  2. fetches the file from the named git tag with `git show <tag>:<file>`
  3. slices the requested line range
  4. compares it line-by-line against the code block that immediately follows
     the caption in the markdown (whitespace-normalised)
  5. prints PASS / SOFT-MATCH / MISMATCH for each, plus a side-by-side diff
     for anything that isn't a clean pass

It also prints WARN when the requested line range is shorter or longer than
what's actually shown in the book listing (a strong signal someone shortened
a listing without updating the caption).

Run from the repo root or from this directory:

    python verify_listings.py              # uses tag v1.0-book
    python verify_listings.py v1.1-book    # check a different tag

Exit codes:
    0 — all clean (PASS or SOFT-MATCH only)
    1 — at least one MISMATCH or BAD-CAPTION
"""

import os
import re
import subprocess
import sys
import difflib

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))

MANUSCRIPT_FILES = [
    "manuscript.md",
    "ch03-05.md",
    "ch06-07.md",
    "ch08-09.md",
    "ch10-11.md",
    "ch12-end.md",
    "ch-appx-c.md",
    "ch-about.md",
]

# Match *Listing N.M. `path/to/file.ext`, lines A-B[ and C-D][: caption text.]*
# Numeric ranges use ASCII hyphen, en-dash (U+2013), or em-dash (U+2014).
CAPTION_RE = re.compile(
    r"^\*Listing\s+([0-9]+\.[0-9]+)\.\s+"   # "Listing 3.1. "
    r"`([^`]+)`,\s+"                         # "`bootloader/boot.asm`, "
    r"(?:lines?|complete)\s+([^:*]+?)"      # "lines 12-19" or "complete"
    r"(?::\s*(.*?))?\.?\*\s*$",             # optional ": caption text.*"
    re.UNICODE,
)

RANGE_RE = re.compile(r"(\d+)\s*[-–—]\s*(\d+)")
SINGLE_RE = re.compile(r"^\s*(\d+)\s*$")

# ANSI colour codes for the terminal report
GREEN  = "\033[92m"
YELLOW = "\033[93m"
RED    = "\033[91m"
DIM    = "\033[2m"
RESET  = "\033[0m"


def normalise(line):
    """Drop trailing whitespace; keep leading whitespace (indentation matters
    inside listings) but collapse internal whitespace runs so a single-space
    vs multi-space drift isn't flagged."""
    return re.sub(r"[ \t]+", " ", line.rstrip())


def parse_ranges(range_str):
    """Parse 'lines 12-19', 'lines 95-102 and 157-165', 'line 42', or
    'complete' into a list of (start, end) tuples. Returns [] for 'complete'
    (whole-file listings are not range-checked)."""
    s = range_str.strip().lower()
    if "complete" in s:
        return []
    pairs = []
    for m in RANGE_RE.finditer(range_str):
        pairs.append((int(m.group(1)), int(m.group(2))))
    if not pairs:
        m = SINGLE_RE.match(range_str)
        if m:
            n = int(m.group(1))
            pairs.append((n, n))
    return pairs


def git_show(tag, path):
    """Return the file's contents at the given tag, or None if missing."""
    try:
        out = subprocess.run(
            ["git", "show", f"{tag}:{path}"],
            cwd=REPO, check=True, capture_output=True, text=True,
            encoding="utf-8", errors="replace",
        )
        return out.stdout
    except subprocess.CalledProcessError:
        return None


def slice_lines(text, ranges):
    """Return the 1-based line slice for each range, concatenated with a
    visual separator between non-contiguous ranges."""
    lines = text.splitlines()
    chunks = []
    for start, end in ranges:
        s = max(0, start - 1)
        e = min(len(lines), end)
        chunks.append("\n".join(lines[s:e]))
    return "\n; ... elsewhere in the file ...\n".join(chunks)


def extract_code_block(md_lines, idx_after_caption):
    """Starting at the line after a caption, find the next fenced code block
    and return its inner contents as a string (without the fence lines)."""
    i = idx_after_caption
    while i < len(md_lines) and not md_lines[i].lstrip().startswith("```"):
        i += 1
    if i >= len(md_lines):
        return None
    i += 1
    start = i
    while i < len(md_lines) and not md_lines[i].lstrip().startswith("```"):
        i += 1
    return "\n".join(md_lines[start:i])


_COMMENT_TAILS = (
    re.compile(r"\s*;[^\"']*$"),     # asm/ld trailing ;
    re.compile(r"\s*//[^\"']*$"),    # C++ trailing //
    re.compile(r"\s*/\*.*\*/\s*$"),  # single-line /* ... */
    re.compile(r"\s*#[^\"']*$"),     # makefile/python trailing #
)


def strip_code_only(lines):
    """Return only the executable part of each line: drop trailing comments
    and skip lines that are pure comments or blank. This is what we use to
    decide 'is this the same code, just annotated differently?'"""
    out = []
    for ln in lines:
        s = ln.strip()
        if not s:
            continue
        # Skip lines that are nothing but a comment.
        if s.startswith((";", "//", "#", "*", "/*")):
            continue
        # Drop any trailing comment.
        for pat in _COMMENT_TAILS:
            s = pat.sub("", s)
        s = s.strip()
        if s:
            out.append(re.sub(r"[ \t]+", " ", s))
    return out


def compare(book_code, source_code):
    """Return a verdict tuple (status, score). Status is one of:
        PASS       — exact (whitespace-normalised) match
        ANNOTATED  — same code, book added/removed comments only
        SOFT-MATCH — close enough to be the same listing
        MISMATCH   — different code (real line-number drift)
    score is informational ratio in [0, 1] on the comment-stripped form."""
    norm_a = [normalise(ln) for ln in book_code.splitlines() if ln.strip()]
    norm_b = [normalise(ln) for ln in source_code.splitlines() if ln.strip()]
    if norm_a == norm_b:
        return "PASS", 1.0
    code_a = strip_code_only(book_code.splitlines())
    code_b = strip_code_only(source_code.splitlines())
    if code_a == code_b:
        return "ANNOTATED", 1.0
    ratio = difflib.SequenceMatcher(None, code_a, code_b).ratio()
    if ratio >= 0.85:
        return "SOFT-MATCH", ratio
    return "MISMATCH", ratio


def main():
    tag = sys.argv[1] if len(sys.argv) > 1 else "v1.0-book"

    print(f"Verifying listings against tag {tag}")
    print(f"Repo root: {REPO}")
    print()

    total = passes = annotated = soft = bad_caption = mismatches = missing = warns = 0
    any_failed = False

    for fname in MANUSCRIPT_FILES:
        path = os.path.join(HERE, fname)
        if not os.path.exists(path):
            continue
        with open(path, encoding="utf-8") as f:
            md = f.read().splitlines()

        for i, line in enumerate(md):
            m = CAPTION_RE.match(line)
            if not m:
                continue
            total += 1
            num, file_path, range_str, caption = m.groups()
            ranges = parse_ranges(range_str)

            source = git_show(tag, file_path)
            if source is None:
                print(f"{RED}MISSING{RESET}  Listing {num}: "
                      f"file '{file_path}' not in tag {tag}")
                print(f"         caption: {(caption or '').strip()}")
                print()
                missing += 1
                any_failed = True
                continue

            book_code = extract_code_block(md, i + 1)
            if book_code is None:
                print(f"{RED}NO-BLOCK{RESET} Listing {num}: "
                      f"no code block found after caption in {fname}")
                bad_caption += 1
                any_failed = True
                continue

            if not ranges:
                # "complete" — verify whole-file match
                source_slice = source.rstrip()
            else:
                source_slice = slice_lines(source, ranges)

            # Warn if the book listing is significantly shorter than the
            # advertised range (somebody trimmed the listing for layout).
            advertised = sum(end - start + 1 for start, end in ranges) \
                         if ranges else len(source_slice.splitlines())
            shown = len([l for l in book_code.splitlines() if l.strip()])
            if ranges and shown + 4 < advertised:
                warns += 1
                print(f"{YELLOW}WARN{RESET}     Listing {num} ({file_path}, "
                      f"lines {range_str.strip()}): advertised "
                      f"{advertised} lines, book shows {shown}")

            verdict, ratio = compare(book_code, source_slice)
            if verdict == "PASS":
                passes += 1
                print(f"{GREEN}PASS{RESET}      Listing {num} "
                      f"({file_path}, lines {range_str.strip()})")
            elif verdict == "ANNOTATED":
                annotated += 1
                print(f"{GREEN}ANNOTATED{RESET} Listing {num} "
                      f"({file_path}, lines {range_str.strip()})  "
                      f"(book added comments; code matches)")
            elif verdict == "SOFT-MATCH":
                soft += 1
                print(f"{DIM}SOFT{RESET}      Listing {num} "
                      f"({file_path}, lines {range_str.strip()})  "
                      f"code-ratio={ratio:.2f}")
            else:
                mismatches += 1
                any_failed = True
                print(f"{RED}MISMATCH{RESET}  Listing {num} "
                      f"({file_path}, lines {range_str.strip()})  "
                      f"code-ratio={ratio:.2f}")
                print(f"  caption: {(caption or '').strip()}")
                _print_diff(book_code, source_slice)

    print()
    print(f"--- summary against {tag} ---")
    print(f"  total listings examined: {total}")
    print(f"  {GREEN}PASS{RESET}       {passes}   (exact match)")
    print(f"  {GREEN}ANNOTATED{RESET}  {annotated}   (book added teaching comments; code matches)")
    print(f"  {DIM}SOFT{RESET}       {soft}   (close enough to be the same listing)")
    print(f"  {YELLOW}WARN{RESET}       {warns}   (book shows fewer lines than advertised)")
    print(f"  {RED}MISMATCH{RESET}   {mismatches}   (line numbers may have drifted)")
    print(f"  {RED}MISSING{RESET}    {missing}   (file not in tag)")
    print(f"  {RED}NO-BLOCK{RESET}   {bad_caption}   (caption with no code block after it)")

    sys.exit(1 if any_failed else 0)


def _print_diff(book_code, source_slice):
    """Compact side-by-side diff for terminal review of a MISMATCH."""
    a = [normalise(ln) for ln in book_code.splitlines()]
    b = [normalise(ln) for ln in source_slice.splitlines()]
    diff = list(difflib.unified_diff(
        b, a, fromfile="repo", tofile="book", lineterm="", n=2,
    ))
    if not diff:
        return
    for line in diff[:30]:
        if line.startswith("+"):
            print(f"  {GREEN}{line}{RESET}")
        elif line.startswith("-"):
            print(f"  {RED}{line}{RESET}")
        else:
            print(f"  {DIM}{line}{RESET}")
    if len(diff) > 30:
        print(f"  ... ({len(diff) - 30} more diff lines)")
    print()


if __name__ == "__main__":
    main()
