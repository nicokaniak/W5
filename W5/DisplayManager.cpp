#include "DisplayManager.h"
#include "RM67162Display.h"
#include "MenuManager.h"
#include <math.h>

static RM67162Display display;

void DisplayManager::initDisplay() {
  display.begin();
  display.setRotation(1);
  display.fillScreen(0x0000); // black
}

void DisplayManager::clearDisplay() {
  display.fillScreen(0x0000);
}

void DisplayManager::drawText(const String &text, int x, int y) {
  display.setTextColor(0xFFFF, 0x0000); // white on black
  display.setTextSize(3);
  display.setCursor(x, y);
  display.print(text);
}

void DisplayManager::drawWatchFace(const String &timeStr) {
  display.fillScreen(0x0000);
  display.setTextColor(0x07FF, 0x0000); // cyan
  display.setTextSize(4);

  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(timeStr, 0, 0, &x1, &y1, &w, &h);
  int16_t x = (display.width() - w) / 2 - x1;
  int16_t y = (display.height() - h) / 2 - y1;
  display.setCursor(x, y);
  display.print(timeStr);
}

void DisplayManager::drawMenu(uint8_t selectedIndex) {
  display.fillScreen(0x0000);  // black

  // Half-circle: center on the LEFT edge (x=0), vertically centered.
  // ponytail: drawCircle draws the full circle; the left half is off-screen so
  // drawPixel's bounds check crops it for free. No arc helper needed.
  // Ceiling: drawCircle is per-pixel via the un-overridden Adafruit_GFX default,
  // ~2*pi*r ~= 690 pixel writes per redraw. Fine for menu (redraw only on change).
  // Upgrade path: override drawFastHLine in RM67162Display to call lcd_fill.
  const int16_t cx = 0;
  const int16_t cy = display.height() / 2;   // 120
  const int16_t r  = 110;
  display.drawCircle(cx, cy, r, 0x8410);     // dim gray arc outline

  // 3 items on the visible right half of the arc:
  //   selected at angle 0   (3 o'clock, rightmost = (r, cy))
  //   previous at -60 deg   (upper)
  //   next     at +60 deg   (lower)
  // ponytail: 60 deg spacing fits 3 items on a 180 deg half-arc with the
  // selected item at the visual focus point (3 o'clock, centered vertically).
  const float anglesDeg[3] = { -60.0f, 0.0f, 60.0f };
  const uint8_t count = MenuManager::menuItemCount();
  uint8_t prevIdx = (selectedIndex + count - 1) % count;
  uint8_t nextIdx = (selectedIndex + 1) % count;
  uint8_t indices[3] = { prevIdx, selectedIndex, nextIdx };

  for (int i = 0; i < 3; i++) {
    float a = anglesDeg[i] * M_PI / 180.0f;
    int16_t ax = cx + (int16_t)(r * cosf(a));
    int16_t ay = cy + (int16_t)(r * sinf(a));
    bool selected = (i == 1);

    String label(MenuManager::menuItemLabel(indices[i]));

    // Selected: size 3 cyan, centered on arc point.
    // Others:   size 2 gray, left-anchored at arc point (text grows rightward into screen).
    // ponytail: left-anchor avoids the long "Configuration" label overflowing off the
    // left edge of the screen when centered at the upper/lower arc points (x ~= 55).
    display.setTextSize(selected ? 3 : 2);
    display.setTextColor(selected ? 0x07FF : 0x8410, 0x0000);  // cyan vs gray

    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(label, 0, 0, &x1, &y1, &w, &h);
    int16_t tx, ty;
    if (selected) {
      tx = ax - w / 2 - x1;
      ty = ay - h / 2 - y1;
    } else {
      tx = ax - x1;            // left edge at arc point
      ty = ay - h / 2 - y1;    // vertically centered on arc point
    }
    display.setCursor(tx, ty);
    display.print(label);
  }
}
