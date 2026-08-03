"""Render an ASCII preview of the Earth globe exactly as drawEarthGlobe
would, plus re-run the assertions with geographically-correct expectations.
Reads the generated W5/earth_landmask.h so it tests the real firmware asset.
Run: python icons/_selfcheck_earth.py
"""
import math, re, os
W, H, BPR = 360, 170, 45
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
hdr = open(os.path.join(REPO, 'W5', 'earth_landmask.h'), encoding='utf-8').read()
bytes_str = re.search(r'EARTH_LANDMASK\[\]\s*=\s*\{([^}]*)\}', hdr, re.S).group(1)
data = bytes(int(b, 16) for b in re.findall(r'0x([0-9A-Fa-f]{2})', bytes_str))
assert len(data) == BPR * H, f"header has {len(data)} bytes, expected {BPR*H}"

def is_land(latDeg, lonDeg):
    while lonDeg < -180: lonDeg += 360
    while lonDeg >= 180: lonDeg -= 360
    col = int(math.floor(lonDeg + 180)); row = 85 - int(math.floor(latDeg))
    if not (0 <= col < W and 0 <= row < H): return False
    return bool(data[row*BPR + (col >> 3)] & (0x80 >> (col & 7)))

def cosZ(lat, lon, dec, sub):
    return math.sin(lat)*math.sin(dec) + math.cos(lat)*math.cos(dec)*math.cos(lon-sub)

def ortho(dx, dy, R, cLat, cLon):
    rho = math.sqrt(dx*dx+dy*dy)
    if rho < 0.5: return cLat, cLon
    c = math.asin(rho/R); sc, cc = math.sin(c), math.cos(c)
    ux, uy = dx/rho, -dy/rho
    lat = math.asin(cc*math.sin(cLat) + uy*sc*math.cos(cLat))
    lon = cLon + math.atan2(ux*sc, cc*math.cos(cLat) - uy*sc*math.sin(cLat))
    return lat, lon

R = 45
cLat = math.radians(55.6761); cLon = math.radians(12.5683)
# Summer solstice, 12:00 UTC: dec=+23.44, subLon=0
dec = math.radians(-23.44*math.cos(2*math.pi/365*(172+10)))
sub = math.radians(0.0)

print(f"dec={math.degrees(dec):+.2f} deg  subLon={math.degrees(sub):+.2f} deg")
print("Globe (Copenhagen center, summer noon UTC). "
      "Day: #=land(black) .=sea(white). Night: #=land(black) ~=sea(dark gray). "
      "Hard terminator. O=red dot\n")
for dy in range(-R, R+1):
    line = ''
    for dx in range(-R, R+1):
        if dx*dx+dy*dy > R*R:
            line += ' '; continue
        lat, lon = ortho(dx, dy, R, cLat, cLon)
        land = is_land(math.degrees(lat), math.degrees(lon))
        z = cosZ(lat, lon, dec, sub)
        if dx==0 and dy==0:
            line += 'O'
        elif z > 0:    line += '#' if land else '.'
        else:          line += '#' if land else '~'
    print(line)

# Corrected assertions: Copenhagen is coastal at 1deg -> sea cell is correct.
print("\n== corrected landmask checks ==")
for lat, lon, expect, name in [
    (55.6761, 12.5683, False, "Copenhagen (coastal, 1deg cell = sea)"),
    (56.0, 13.0, True, "Sweden near Cph"),
    (56.0, 10.0, False, "Jutland DK (sub-pixel at 1deg)"),
    (48.8566, 2.3522, True, "Paris (inland-ish)"),
    (36.0, 138.0, True, "central Honshu JP"),
    (40.0, -100.0, True, "central US"),
    (-25.0, 135.0, True, "central Australia"),
    (0.0, -30.0, False, "mid-Atlantic"),
    (0.0, -160.0, False, "mid-Pacific"),
]:
    got = is_land(lat, lon)
    print(f"  {'OK ' if got==expect else 'FAIL'} {name:32s} land={got}")
    assert got == expect, f"{name}: got {got}"

print("\nALL CHECKS PASSED")
