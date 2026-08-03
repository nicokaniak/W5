#!/usr/bin/env python3
# ponytail: TTF-based letter glyph generator for the W5 watch.
# Renders A-Z, a-z, and punctuation from DYLOVA5TUFF.ttf into 1-bit PROGMEM
# bitmaps matching the fd_ (33x53), dd_ (16x25), td_ (8x12) box formats.
# Digits 0-9 stay procedural 7-seg (they're not used in the main render path —
# digits come from starfield_icons.h via fdDigits/ddDigits).
#
# Run:  python icons/gen_font_letters.py   (from repo root)
# Writes: W5/sevenseg_letters.h
import os
from PIL import Image, ImageDraw, ImageFont

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TTF  = os.path.join(REPO, "W5", "Fonts", "DYLOVA5TUFF-V05", "DYLOVA5TUFF.ttf")
OUT  = os.path.join(REPO, "W5", "sevenseg_letters.h")

# Characters to render with the TTF font.
FONT_CHARS = set(
    list("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz")
    + list(".,'!?:-/()")  # punctuation the font likely has
)

# Characters that stay procedural 7-seg (digits + hard punctuation).
SEGMENT_CHARS = {
    '0': "abcdef", '1': "bc", '2': "abdeg", '3': "abcdg",
    '4': "bcfg", '5': "acdfg", '6': "acdefg", '7': "abc",
    '8': "abcdefg", '9': "abcdfg",
    '-': "g", '_': "d",
}

# --- procedural 7-seg helpers (kept for digits + segment punctuation) ---

def make_segments(w, h, t):
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
        'a': hseg(0), 'g': hseg(mid - t // 2), 'd': hseg(h - t),
        'f': vseg(0, v_y0, mid), 'b': vseg(w - t, v_y0, mid),
        'e': vseg(0, mid, v_y1), 'c': vseg(w - t, mid, v_y1),
    }

def px_period(w, h, t):
    s = max(2, t - 1)
    x0 = w - s - 2; y0 = h - s - 1
    return {(x, y) for x in range(x0, x0 + s) for y in range(y0, y0 + s)}

def px_comma(w, h, t):
    s = max(2, t - 1)
    x0 = w - s - 2; y0 = h - s - 2
    px = {(x, y) for x in range(x0, x0 + s) for y in range(y0, y0 + s)}
    px.add((x0 - 1, y0 + s)); px.add((x0 - 2, y0 + s + 1))
    return px

def px_apostrophe(w, h, t):
    s = max(2, t - 1)
    x0 = w - s - 2; y0 = 1
    return {(x, y) for x in range(x0, x0 + s) for y in range(y0, y0 + s)}

def px_slash(w, h, t):
    px = set()
    for y in range(h):
        x = w - 2 - y * (w - 4) // max(1, h - 1)
        if 0 <= x < w:
            px.add((x, y))
            if x - 1 >= 0: px.add((x - 1, y))
    return px

def px_exclaim(w, h, t):
    segs = make_segments(w, h, t)
    px = set(segs['b'])
    s = max(2, t - 1); cx = w // 2
    for x in range(cx - s // 2, cx + s // 2 + 1):
        for y in range(h - s - 1, h - 1):
            if 0 <= x < w and 0 <= y < h: px.add((x, y))
    return px

def px_question(w, h, t):
    segs = make_segments(w, h, t)
    px = segs['a'] | segs['b'] | segs['g'] | segs['f']
    s = max(2, t - 1); cx = w // 2
    for x in range(cx - s // 2, cx + s // 2 + 1):
        for y in range(h - s - 1, h - 1):
            if 0 <= x < w and 0 <= y < h: px.add((x, y))
    return px

def px_percent(w, h, t):
    px = set(); s = max(2, t - 1)
    for x in range(2, 2 + s):
        for y in range(2, 2 + s): px.add((x, y))
    for x in range(w - s - 2, w - 2):
        for y in range(h - s - 1, h - 1): px.add((x, y))
    for i in range(min(w, h)):
        x = 2 + i * (w - 4) // max(1, min(w, h) - 1)
        y = 2 + i * (h - 4) // max(1, min(w, h) - 1)
        if 0 <= x < w and 0 <= y < h: px.add((x, y))
    return px

def px_colon(w, h, t):
    px = set(); s = max(2, t - 1); cx = w // 2
    for yc in (h // 3, h * 2 // 3):
        for x in range(cx - s // 2, cx + s // 2 + 1):
            for y in range(yc - s // 2, yc + s // 2 + 1):
                if 0 <= x < w and 0 <= y < h: px.add((x, y))
    return px

PIXEL_CHARS = {
    '.': px_period, ',': px_comma, "'": px_apostrophe,
    '/': px_slash, '!': px_exclaim, '?': px_question,
    '%': px_percent, ':': px_colon,
}

# --- TTF rendering ---

def render_ttf_glyph(ch, w, h, font_size):
    """Render a single character from the TTF font, centered in a w*h box.
    Returns a set of (x, y) pixels (on-pixels)."""
    font = ImageFont.truetype(TTF, font_size)
    # Render on a large enough canvas to capture the full glyph
    canvas_w, canvas_h = w * 3, h * 3
    img = Image.new('L', (canvas_w, canvas_h), 255)
    draw = ImageDraw.Draw(img)
    # Get glyph bbox to center it
    bbox = font.getbbox(ch)
    gw = bbox[2] - bbox[0]
    gh = bbox[3] - bbox[1]
    # Center in the target box
    tx = (canvas_w - gw) // 2 - bbox[0]
    ty = (canvas_h - gh) // 2 - bbox[1]
    draw.text((tx, ty), ch, font=font, fill=0)
    # Crop to the target box size centered in the canvas
    cx0 = (canvas_w - w) // 2
    cy0 = (canvas_h - h) // 2
    crop = img.crop((cx0, cy0, cx0 + w, cy0 + h))
    # Threshold to 1-bit and collect on-pixels
    px = set()
    for y in range(h):
        for x in range(w):
            if crop.getpixel((x, y)) < 128:
                px.add((x, y))
    return px

# --- bitmap packing ---

def pack_pixels(px_set, w, h):
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

def pack_segments(segs, active, w, h):
    px = set()
    for s in active:
        px |= segs.get(s, set())
    return pack_pixels(px, w, h)

# --- C header emission ---

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
    if ch == "'": return "'\\''"
    if ch == '\\': return "'\\\\'"
    if ch == '\n': return "'\\n'"
    if ch == '\t': return "'\\t'"
    return f"'{ch}'"

def lookup_function(prefix, chars, box_w, box_h):
    """Generate the switch-case lookup function for a given prefix."""
    base = prefix.rstrip('_')
    fname = f"{base}_letterBitmap"
    lines = [
        f"// {base} glyph lookup: char -> bitmap, or nullptr.",
        f"inline const unsigned char* {fname}(char ch) {{",
        "  switch (ch) {",
    ]
    for ch in sorted(chars):
        code = ord(ch)
        lines.append(f"    case {c_char_literal(ch)}: return {base}_glyph_{code};")
    lines.append("  }")
    lines.append("  return nullptr;")
    lines.append("}")
    return "\n".join(lines)

def main():
    # (prefix, width, height, segment_thickness, font_size)
    sizes = [
        ("fd_", 33, 53, 6, 52),
        ("dd_", 16, 25, 3, 25),
        ("td_",  8, 12, 2, 12),
    ]

    all_chars = sorted(set(
        list(FONT_CHARS) + list(SEGMENT_CHARS.keys()) + list(PIXEL_CHARS.keys())
    ))

    chunks = [
        "// AUTO-GENERATED by icons/gen_font_letters.py -- do not edit by hand.",
        "// 1-bit font: A-Z, a-z, and punctuation rendered from DYLOVA5TUFF.ttf.",
        "// Digits 0-9 are procedural 7-seg (main render path uses starfield_icons.h).",
        f"// fd_ glyphs are 33x53, dd_ glyphs are 16x25, td_ glyphs are 8x12.",
        "#ifndef SEVENSEG_LETTERS_H",
        "#define SEVENSEG_LETTERS_H",
        "#include <Arduino.h>",
        "",
    ]

    for prefix, w, h, t, font_sz in sizes:
        segs = make_segments(w, h, t)
        for ch in all_chars:
            if ch in FONT_CHARS:
                px = render_ttf_glyph(ch, w, h, font_sz)
                data = pack_pixels(px, w, h)
            elif ch in SEGMENT_CHARS:
                data = pack_segments(segs, SEGMENT_CHARS[ch], w, h)
            elif ch in PIXEL_CHARS:
                px = PIXEL_CHARS[ch](w, h, t)
                data = pack_pixels(px, w, h)
            else:
                continue
            code = ord(ch)
            name = f"{prefix.rstrip('_')}_glyph_{code}"
            chunks.append(f"// {c_char_literal(ch)}, {w}x{h}px")
            chunks.append(emit(name, w, h, data))
            chunks.append("")
        chunks.append(lookup_function(prefix, all_chars, w, h))
        chunks.append("")

    chunks.append("#endif // SEVENSEG_LETTERS_H")

    with open(OUT, "w", newline="\n") as f:
        f.write("\n".join(chunks) + "\n")
    print(f"Wrote {OUT}")
    print(f"  {len(all_chars)} chars x {len(sizes)} sizes = {len(all_chars) * len(sizes)} glyphs")

if __name__ == "__main__":
    main()
