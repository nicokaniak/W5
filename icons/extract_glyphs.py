#!/usr/bin/env python3
"""Crop individual glyphs from the NB Architekt font specimen PNG,
using glyph positions extracted from the SVG version."""
import xml.etree.ElementTree as ET
import re
import os
from collections import defaultdict
from PIL import Image, ImageOps

SVG_PATH = r'C:\Users\Nicolas Kaniak\Documents\Arduino\W5\W5\Fonts\Nb Architekt R Regular font.svg'
PNG_PATH = r'C:\Users\Nicolas Kaniak\Documents\Arduino\W5\W5\Fonts\Nb Architekt R Regular font.png'
OUT_DIR = r'C:\Users\Nicolas Kaniak\Documents\Arduino\W5\W5\Fonts\glyphs'
os.makedirs(OUT_DIR, exist_ok=True)

tree = ET.parse(SVG_PATH)
root = tree.getroot()
ns = {'svg': 'http://www.w3.org/2000/svg'}
paths = root.findall('.//svg:path', ns)

dark = []
for p in paths[1:]:
    tf = p.get('transform', '')
    fill = p.get('fill', '')
    m = re.search(r'translate\(([\d.]+),([\d.]+)\)', tf)
    if m:
        x, y = float(m.group(1)), float(m.group(2))
        r = int(fill[1:3], 16) if fill.startswith('#') else 0
        if r < 128:
            dark.append({'x': x, 'y': y})

rows = defaultdict(list)
for p in dark:
    row_key = round(p['y'] / 10) * 10
    rows[row_key].append(p)

im = Image.open(PNG_PATH).convert('L')

# Crop box half-sizes per row (generous, then trim)
ROW_PARAMS = {
    20:  (20, 24),   # rows 1-4: ~45px tall
    100: (20, 24),
    180: (20, 24),
    270: (20, 24),
    350: (22, 28),   # row 5: ~53px tall
    430: (25, 38),   # row 6: ~72px tall, variable y
    440: (25, 38),
    450: (25, 38),
    460: (25, 38),
    480: (25, 38),
}

row_num = 0
total = 0
for rk in sorted(rows.keys()):
    row_num += 1
    glyphs_in_row = sorted(rows[rk], key=lambda p: p['x'])
    hw, hh = ROW_PARAMS.get(rk, (20, 24))
    for col, g in enumerate(glyphs_in_row):
        cx, cy = int(g['x']), int(g['y'])
        box = (cx - hw, cy - hh, cx + hw, cy + hh)
        box = (max(0, box[0]), max(0, box[1]),
               min(im.width, box[2]), min(im.height, box[3]))
        crop = im.crop(box)
        # Trim white borders
        bg = Image.new('L', crop.size, 255)
        diff = Image.new('L', crop.size)
        for x in range(crop.width):
            for y in range(crop.height):
                diff.putpixel((x, y), 255 if crop.getpixel((x, y)) < 128 else 0)
        bbox = diff.getbbox()
        if bbox:
            crop = crop.crop(bbox)
        # Save inverted (dark glyph on white -> white glyph on black for visibility)
        crop.save(os.path.join(OUT_DIR, 'r%d_c%02d.png' % (row_num, col)))
        total += 1

print('Saved %d glyph images to %s' % (total, OUT_DIR))
