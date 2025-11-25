#include "DisplayManager.h"
#include "RM67162Display.h"

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
