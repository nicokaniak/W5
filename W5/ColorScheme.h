#ifndef COLORSCHEME_H
#define COLORSCHEME_H

#include <Arduino.h>

// ponytail: persisted HSV color theme. The user picks one main color; the rest
// of the UI is generated from it via color-theory rules.

enum ColorSchemeType {
  SCHEME_MONOCHROMATIC = 0,
  SCHEME_ANALOGOUS,
  SCHEME_COMPLEMENTARY,
  SCHEME_TRIADIC,
  SCHEME_SPLIT_COMPLEMENTARY,
  SCHEME_TETRADIC,
  SCHEME_TYPE_COUNT
};

struct ColorHSV {
  uint16_t h; // 0..359
  uint8_t  s; // 0..255
  uint8_t  v; // 0..255
};

struct ColorPalette {
  uint16_t primary;    // main accent (replaces cyan)
  uint16_t secondary;  // titles / date / sun labels (replaces light blue-white)
  uint16_t success;    // running / short break (replaces green)
  uint16_t warning;    // lap / session / sun icon (replaces yellow/orange)
  uint16_t error;      // stopped / work / temp line (replaces red)
  uint16_t info;       // precipitation / low temp / long break (replaces light blue)
  uint16_t text;       // white text
  uint16_t light;      // near-white text with a subtle hue tint
  uint16_t dim;        // gray text / brackets
  uint16_t muted;      // dark gray / scanlines / unselected dots
  uint16_t accent;     // bright highlight
};

namespace ColorScheme {
  void init();                               // load from NVS, build palette
  void save();                               // write current to NVS
  const char* typeLabel(ColorSchemeType t);  // short name for UI
  uint8_t typeCount();

  ColorSchemeType currentType();
  ColorHSV        currentHSV();
  const ColorPalette& currentPalette();      // applied/current palette

  void setType(ColorSchemeType t);           // update applied type + palette
  void setHSV(const ColorHSV &hsv);          // update applied color + palette

  uint16_t hsvTo565(uint16_t h, uint8_t s, uint8_t v);
  void generatePalette(const ColorHSV &main, ColorSchemeType type,
                       ColorPalette &out);
}

#endif
