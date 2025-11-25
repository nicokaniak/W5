#include "DisplayManager.h"
#include <TFT_eSPI.h> // Library for Lilygo T-Display AMOLED

static TFT_eSPI tft = TFT_eSPI();

void DisplayManager::initDisplay() {
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
}

void DisplayManager::clearDisplay() {
  tft.fillScreen(TFT_BLACK);
}

void DisplayManager::drawText(const String &text, int x, int y) {
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(3);
  tft.drawString(text, x, y);
}

void DisplayManager::drawWatchFace(const String &timeStr) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextSize(4);

  auto previousDatum = tft.getTextDatum();
  tft.setTextDatum(MC_DATUM);
  tft.drawString(timeStr, tft.width() / 2, tft.height() / 2);
  tft.setTextDatum(previousDatum);
}
