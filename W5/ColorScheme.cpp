#include "ColorScheme.h"
#include <Preferences.h>

static ColorSchemeType s_type;
static ColorHSV        s_hsv;
static ColorPalette    s_palette;

static const char* SCHEME_LABELS[] = {
  "MONOCHROME",
  "ANALOGOUS",
  "COMPLEMENT",
  "TRIADIC",
  "SPLIT",
  "TETRADIC"
};

static int16_t wrapHue(int16_t h) {
  while (h < 0) h += 360;
  while (h >= 360) h -= 360;
  return h;
}

static void hsvToRgb(uint16_t h, uint8_t s, uint8_t v,
                     uint8_t &r, uint8_t &g, uint8_t &b) {
  uint16_t hh = h % 360;
  uint8_t region = hh / 60;
  uint16_t remainder = (hh - (uint16_t)region * 60) * 255 / 60;
  uint8_t p = (uint16_t)v * (255 - s) / 255;
  uint8_t q = (uint16_t)v * (255 - ((uint16_t)s * remainder / 255)) / 255;
  uint8_t t = (uint16_t)v * (255 - ((uint16_t)s * (255 - remainder) / 255)) / 255;
  switch (region % 6) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default: r = v; g = p; b = q; break;
  }
}

uint16_t ColorScheme::hsvTo565(uint16_t h, uint8_t s, uint8_t v) {
  uint8_t r, g, b;
  hsvToRgb(h, s, v, r, g, b);
  return (uint16_t)(((r >> 3) & 0x1F) << 11) |
                  (((g >> 2) & 0x3F) << 5)  |
                   ((b >> 3) & 0x1F);
}

void ColorScheme::generatePalette(const ColorHSV &main, ColorSchemeType type,
                                  ColorPalette &out) {
  uint16_t h = main.h % 360;
  uint8_t  s = main.s;
  uint8_t  v = main.v;

  // Neutrals with a subtle tint from the main hue.
  out.text  = hsvTo565(0, 0, 255);
  out.dim   = hsvTo565(h, 40, 170);
  out.muted = hsvTo565(h, 30, 70);
  out.accent = hsvTo565(h, s, v > 235 ? 255 : v + 20);

  // info: for monochromatic use a desaturated main; otherwise the complement.
  if (type == SCHEME_MONOCHROMATIC) {
    out.info = hsvTo565(h, (uint8_t)((uint16_t)s * 5 / 10), v);
  } else {
    out.info = hsvTo565(wrapHue(h + 180),
                        (uint8_t)((uint16_t)s * 7 / 10),
                        (uint8_t)((uint16_t)v * 9 / 10));
  }

  int16_t h1, h2, h3, h4, h5;
  uint8_t s1, s2, s3, s4, s5;
  uint8_t v1, v2, v3, v4, v5;

  switch (type) {
    case SCHEME_MONOCHROMATIC:
      h1 = 0;   s1 = s;                                       v1 = v;
      h2 = 0;   s2 = (uint8_t)((uint16_t)s * 7 / 10);         v2 = v;
      h3 = 0;   s3 = (uint8_t)((uint16_t)s * 6 / 10);         v3 = (uint8_t)((uint16_t)v * 85 / 100);
      h4 = 0;   s4 = s;                                       v4 = (uint8_t)((uint16_t)v * 75 / 100);
      h5 = 0;   s5 = s;                                       v5 = (uint8_t)((uint16_t)v * 55 / 100);
      break;

    case SCHEME_ANALOGOUS:
      h1 = 0;   s1 = s;   v1 = v;
      h2 = +30; s2 = s;   v2 = v;
      h3 = -30; s3 = s;   v3 = v;
      h4 = +60; s4 = s;   v4 = v;
      h5 = -60; s5 = s;   v5 = (uint8_t)((uint16_t)v * 85 / 100);
      break;

    case SCHEME_COMPLEMENTARY:
      h1 = 0;    s1 = s;   v1 = v;
      h2 = +180; s2 = s;   v2 = v;
      h3 = +180; s3 = (uint8_t)((uint16_t)s * 7 / 10); v3 = v;
      h4 = +30;  s4 = s;   v4 = v;
      h5 = +180; s5 = s;   v5 = v;
      break;

    case SCHEME_TRIADIC:
      h1 = 0;    s1 = s;   v1 = v;
      h2 = +120; s2 = s;   v2 = v;
      h3 = -120; s3 = s;   v3 = v;
      h4 = +120; s4 = (uint8_t)((uint16_t)s * 8 / 10); v4 = v;
      h5 = -120; s5 = s;   v5 = (uint8_t)((uint16_t)v * 85 / 100);
      break;

    case SCHEME_SPLIT_COMPLEMENTARY:
      h1 = 0;    s1 = s;   v1 = v;
      h2 = +150; s2 = s;   v2 = v;
      h3 = -150; s3 = s;   v3 = v;
      h4 = +150; s4 = (uint8_t)((uint16_t)s * 8 / 10); v4 = (uint8_t)((uint16_t)v * 9 / 10);
      h5 = -150; s5 = s;   v5 = v;
      break;

    case SCHEME_TETRADIC:
      h1 = 0;    s1 = s;   v1 = v;
      h2 = +180; s2 = s;   v2 = v;
      h3 = +90;  s3 = s;   v3 = v;
      h4 = +270; s4 = s;   v4 = v;
      h5 = +180; s5 = s;   v5 = (uint8_t)((uint16_t)v * 9 / 10);
      break;

    default:
      h1 = 0; s1 = s; v1 = v;
      h2 = 0; s2 = s; v2 = v;
      h3 = 0; s3 = s; v3 = v;
      h4 = 0; s4 = s; v4 = v;
      h5 = 0; s5 = s; v5 = v;
      break;
  }

  out.primary   = hsvTo565(wrapHue(h + h1), s1, v1);
  out.secondary = hsvTo565(wrapHue(h + h2), s2, v2);
  out.success   = hsvTo565(wrapHue(h + h3), s3, v3);
  out.warning   = hsvTo565(wrapHue(h + h4), s4, v4);
  out.error     = hsvTo565(wrapHue(h + h5), s5, v5);
}

const char* ColorScheme::typeLabel(ColorSchemeType t) {
  if ((uint8_t)t >= SCHEME_TYPE_COUNT) return "?";
  return SCHEME_LABELS[(uint8_t)t];
}

uint8_t ColorScheme::typeCount() { return SCHEME_TYPE_COUNT; }

void ColorScheme::init() {
  Preferences prefs;
  prefs.begin("w5", true); // read-only
  uint16_t h = prefs.getUShort("cs_h", 180);
  uint8_t  s = prefs.getUChar("cs_s", 255);
  uint8_t  v = prefs.getUChar("cs_v", 255);
  uint8_t  t = prefs.getUChar("cs_type", (uint8_t)SCHEME_COMPLEMENTARY);
  prefs.end();

  if (h >= 360) h = 180;
  if (s > 255) s = 255;
  if (v > 255) v = 255;
  if (t >= (uint8_t)SCHEME_TYPE_COUNT) t = (uint8_t)SCHEME_COMPLEMENTARY;

  s_hsv = {h, s, v};
  s_type = (ColorSchemeType)t;
  generatePalette(s_hsv, s_type, s_palette);
}

void ColorScheme::save() {
  Preferences prefs;
  prefs.begin("w5", false); // read-write
  prefs.putUShort("cs_h", s_hsv.h);
  prefs.putUChar("cs_s", s_hsv.s);
  prefs.putUChar("cs_v", s_hsv.v);
  prefs.putUChar("cs_type", (uint8_t)s_type);
  prefs.end();
  Serial.printf("COLOR: saved h=%u s=%u v=%u type=%u\n",
                s_hsv.h, s_hsv.s, s_hsv.v, (uint8_t)s_type);
}

ColorSchemeType ColorScheme::currentType() { return s_type; }
ColorHSV        ColorScheme::currentHSV()  { return s_hsv; }
const ColorPalette& ColorScheme::currentPalette() { return s_palette; }

void ColorScheme::setType(ColorSchemeType t) {
  if ((uint8_t)t >= SCHEME_TYPE_COUNT) return;
  s_type = t;
  generatePalette(s_hsv, s_type, s_palette);
}

void ColorScheme::setHSV(const ColorHSV &hsv) {
  s_hsv = hsv;
  s_hsv.h %= 360;
  if (s_hsv.s > 255) s_hsv.s = 255;
  if (s_hsv.v > 255) s_hsv.v = 255;
  generatePalette(s_hsv, s_type, s_palette);
}
