#!/usr/bin/env python3
# ponytail: procedural 7-segment font generator. No PNG source — segments
# are computed from geometry and packed to 1-bit PROGMEM matching the fd_
# (33x53) and dd_ (16x25) digit formats in starfield_icons.h.
# Run:  python icons/gen_7seg_letters.py   (from repo root)
# Writes: W5/sevenseg_letters.h
import os

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(REPO, "W5", "sevenseg_letters.h")

# Segment-based characters. Segments: a(top) b(top-right) c(bot-right)
# d(bottom) e(bot-left) f(top-left) g(mid)
# Full A-Z + a-z + segment-based punctuation. Hard letters (M,V,W,K,X) use
# best-effort approximations — they're inherently ambiguous in 7-seg.
SEGMENT_CHARS = {
    # Digits 0-9 (generated — simpler than starfield_icons.h but consistent
    # with the letter style. Only used for the tiny td_ font; fd_/dd_ digits
    # still come from starfield_icons.h for the beveled look.)
    '0': "abcdef", '1': "bc", '2': "abdeg", '3': "abcdg",
    '4': "bcfg", '5': "acdfg", '6': "acdefg", '7': "abc",
    '8': "abcdefg", '9': "abcdfg",
    # Uppercase A-Z
    'A': "abcefg", 'B': "cdefg", 'C': "adef",  'D': "bcdeg",
    'E': "adefg",  'F': "aefg",  'G': "abcdf", 'H': "bcefg",
    'I': "ef",     'J': "bcde",  'K': "cefg",  'L': "def",
    'M': "abcef",  'N': "ceg",   'O': "abcdef",'P': "abefg",
    'Q': "abcdfg", 'R': "eg",    'S': "acdfg", 'T': "defg",
    'U': "bcdef",  'V': "cde",   'W': "cde",   'X': "bcefg",
    'Y': "bcdfg",  'Z': "abdeg",
    # Lowercase a-z (where different from uppercase)
    'a': "abcefg", 'b': "cdefg", 'c': "deg",   'd': "bcdeg",
    'e': "adefg",  'f': "aefg",  'g': "abcdf", 'h': "cefg",
    'i': "ef",     'j': "bcde",  'k': "cefg",  'l': "ef",
    'm': "ceg",    'n': "ceg",   'o': "cdeg",  'p': "abefg",
    'q': "abcfg",  'r': "eg",    's': "acdfg", 't': "defg",
    'u': "cde",    'v': "cde",   'w': "cde",   'x': "bcefg",
    'y': "bcdfg",  'z': "abdeg",
    # Segment-based punctuation
    '-': "g", '_': "d", '(': "ef", ')': "bc",
}


def make_segments(w, h, t):
    """Return dict segment_char -> set of (x,y) pixels for a w*h grid.

    Layout (standard 7-seg):
      _aaa_         h_x0..h_x1 = horizontal span
     |f   b|        v_y0..v_y1 = vertical span
     |f   b|        t          = segment thickness
      _ggg_         mid        = vertical midpoint
     |e   c|
     |e   c|
      _ddd_
    """
    h_x0 = t // 2
    h_x1 = w - 1 - t // 2
    v_y0 = t // 2
    v_y1 = h - 1 - t // 2
    mid = h // 2

    def hseg(y0):
        return {(x, y) for y in range(y0, y0 + t) for x in range(h_x0, h_x1 + 1)}

    def vseg(x0, y0, y1):
        return {(x, y) for x in range(x0, x0 + t) for y in range(y0, y1 + 1)}

    return {
        'a': hseg(0),
        'g': hseg(mid - t // 2),
        'd': hseg(h - t),
        'f': vseg(0, v_y0, mid),
        'b': vseg(w - t, v_y0, mid),
        'e': vseg(0, mid, v_y1),
        'c': vseg(w - t, mid, v_y1),
    }


# --- Pixel-based punctuation (can't be done with segments alone) ---

def px_period(w, h, t):
    """Period: filled square at bottom-right."""
    s = max(2, t - 1)
    x0 = w - s - 2
    y0 = h - s - 1
    return {(x, y) for x in range(x0, x0 + s) for y in range(y0, y0 + s)}

def px_comma(w, h, t):
    """Comma: dot at bottom-right with a small tail going down-left."""
    s = max(2, t - 1)
    x0 = w - s - 2
    y0 = h - s - 2
    px = {(x, y) for x in range(x0, x0 + s) for y in range(y0, y0 + s)}
    px.add((x0 - 1, y0 + s))
    px.add((x0 - 2, y0 + s + 1))
    return px

def px_apostrophe(w, h, t):
    """Apostrophe: small dot at top-right."""
    s = max(2, t - 1)
    x0 = w - s - 2
    y0 = 1
    return {(x, y) for x in range(x0, x0 + s) for y in range(y0, y0 + s)}

def px_slash(w, h, t):
    """Slash: diagonal from bottom-left to top-right, 2px thick."""
    px = set()
    for y in range(h):
        x = w - 2 - y * (w - 4) // max(1, h - 1)
        if 0 <= x < w:
            px.add((x, y))
            if x - 1 >= 0:
                px.add((x - 1, y))
    return px

def px_exclaim(w, h, t):
    """Exclamation: b segment (top-right vertical) + dot at bottom-center."""
    segs = make_segments(w, h, t)
    px = set(segs['b'])
    s = max(2, t - 1)
    cx = w // 2
    for x in range(cx - s // 2, cx + s // 2 + 1):
        for y in range(h - s - 1, h - 1):
            if 0 <= x < w and 0 <= y < h:
                px.add((x, y))
    return px

def px_question(w, h, t):
    """Question mark: a,b,g,f segments (top hook) + dot at bottom-center."""
    segs = make_segments(w, h, t)
    px = segs['a'] | segs['b'] | segs['g'] | segs['f']
    s = max(2, t - 1)
    cx = w // 2
    for x in range(cx - s // 2, cx + s // 2 + 1):
        for y in range(h - s - 1, h - 1):
            if 0 <= x < w and 0 <= y < h:
                px.add((x, y))
    return px

def px_percent(w, h, t):
    """Percent: two dots (top-left, bottom-right) + diagonal."""
    px = set()
    s = max(2, t - 1)
    for x in range(2, 2 + s):
        for y in range(2, 2 + s):
            px.add((x, y))
    for x in range(w - s - 2, w - 2):
        for y in range(h - s - 1, h - 1):
            px.add((x, y))
    for i in range(min(w, h)):
        x = 2 + i * (w - 4) // max(1, min(w, h) - 1)
        y = 2 + i * (h - 4) // max(1, min(w, h) - 1)
        if 0 <= x < w and 0 <= y < h:
            px.add((x, y))
    return px

def px_colon(w, h, t):
    """Colon: two dots vertically centered (for font completeness)."""
    px = set()
    s = max(2, t - 1)
    cx = w // 2
    for yc in (h // 3, h * 2 // 3):
        for x in range(cx - s // 2, cx + s // 2 + 1):
            for y in range(yc - s // 2, yc + s // 2 + 1):
                if 0 <= x < w and 0 <= y < h:
                    px.add((x, y))
    return px

PIXEL_CHARS = {
    '.': px_period,  ',': px_comma,    "'": px_apostrophe,
    '/': px_slash,   '!': px_exclaim,  '?': px_question,
    '%': px_percent, ':': px_colon,
}


def render_pixels(px_set, w, h):
    """Pack a set of (x,y) pixels into MSB-first 1-bit bytes, row-padded."""
    bpr = (w + 7) // 8
    out = []
    for y in range(h):
        for bi in range(bpr):
            b = 0
            for bit in range(8):
                x = bi * 8 + bit
                if x < w and (x, y) in px_set:
                    b |= 0x80 >> bit
            out.append(b)
    return out


def render_segments(segs, active, w, h):
    """Pack active segments into MSB-first 1-bit bytes, row-padded."""
    px = set()
    for s in active:
        px |= segs.get(s, set())
    return render_pixels(px, w, h)


def emit(name, w, h, data):
    lines = [f"static const unsigned char PROGMEM {name}[] = {{"]
    row = []
    for b in data:
        row.append(f"0x{b:02X}")
        if len(row) == 12:
            lines.append("\t" + ", ".join(row) + ",")
            row = []
    if row:
        lines.append("\t" + ", ".join(row))
    lines.append("};")
    return "\n".join(lines)


def c_char_literal(ch):
    """Safely emit a C char literal for any character."""
    if ch == "'":
        return "'\\''"
    if ch == '\\':
        return "'\\\\'"
    if ch == '\n':
        return "'\\n'"
    if ch == '\t':
        return "'\\t'"
    return f"'{ch}'"


def main():
    sizes = [
        ("fd_", 33, 53, 6),
        ("dd_", 16, 25, 3),
        ("td_",  8, 12, 2),  # tiny — matches former dotText size 1 (~6x8)
    ]

    all_chars = sorted(set(list(SEGMENT_CHARS.keys()) + list(PIXEL_CHARS.keys())))

    chunks = [
        "// AUTO-GENERATED by icons/gen_7seg_letters.py -- do not edit by hand.",
        "// 1-bit 7-segment font: full A-Z, a-z, digits via starfield_icons.h,",
        "// plus punctuation: . , - ' / ! ? % ( ) : _",
        "// fd_ glyphs are 33x53 (match big digits), dd_ glyphs are 16x25",
        "// (match small digits). Hard letters (M,V,W,K,X) are best-effort.",
        "#ifndef SEVENSEG_LETTERS_H",
        "#define SEVENSEG_LETTERS_H",
        "#include <Arduino.h>",
        "",
    ]

    for prefix, w, h, t in sizes:
        segs = make_segments(w, h, t)
        for ch in all_chars:
            if ch in SEGMENT_CHARS:
                data = render_segments(segs, SEGMENT_CHARS[ch], w, h)
            else:
                data = render_pixels(PIXEL_CHARS[ch](w, h, t), w, h)
            desc = repr(ch) if ch == "'" else f"'{ch}'"
            chunks.append(f"// {desc}, {w}x{h}px")
            chunks.append(emit(f"{prefix}glyph_{ord(ch)}", w, h, data))
            chunks.append("")

        chunks.append(f"// {prefix} glyph lookup: char -> bitmap, or nullptr.")
        chunks.append(f"// Handles A-Z, a-z, and punctuation. Digits (0-9) are")
        chunks.append(f"// NOT here — use fdDigits/ddDigits from starfield_icons.h.")
        chunks.append(f"inline const unsigned char* {prefix}letterBitmap(char ch) {{")
        chunks.append("  switch (ch) {")
        for ch in all_chars:
            chunks.append(f"    case {c_char_literal(ch)}: return {prefix}glyph_{ord(ch)};")
        chunks.append("  }")
        chunks.append("  return nullptr;")
        chunks.append("}")
        chunks.append("")

    chunks.append("#endif  // SEVENSEG_LETTERS_H")

    with open(OUT, "w", newline="\n") as f:
        f.write("\n".join(chunks) + "\n")
    n_letters = len(SEGMENT_CHARS)
    n_punct = len(PIXEL_CHARS)
    print(f"Wrote {OUT}  ({n_letters} segment + {n_punct} pixel glyphs x {len(sizes)} sizes)")


if __name__ == "__main__":
    main()
