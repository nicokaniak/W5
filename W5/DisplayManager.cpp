#include "DisplayManager.h"
#include "BatteryManager.h"
#include "RM67162Display.h"


static RM67162Display display;

void DisplayManager::initDisplay() {
  display.begin();
  display.setRotation(1);
  display.fillScreen(0x0000); // black
}

void DisplayManager::clearDisplay() { display.fillScreen(0x0000); }

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

  // ----- Battery bar (bottom‑right) -----
  int batPct = BatteryManager::getPercentage();
  int barWidth = map(batPct, 0, 100, 0, 50); // max 50 px width
  int barX = display.width() - 55;           // 5 px margin from right edge
  int barY = display.height() - 10;          // 10 px from bottom
  // background (light gray)
  display.fillRect(barX, barY, 50, 8, 0x7BEF);
  // fill proportional to charge (green)
  display.fillRect(barX, barY, barWidth, 8, 0x07E0);
}
