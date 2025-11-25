#include "RM67162Display.h"
#include "rm67162.h"
#include <Arduino.h>

RM67162Display::RM67162Display()
    : Adafruit_GFX(RM67162_PANEL_WIDTH, RM67162_PANEL_HEIGHT) {}

void RM67162Display::begin() {
  Serial.println("RM67162Display::begin() starting...");

  // Power enable pin (GPIO 38) - critical for AMOLED
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);

  delay(100);

  rm67162_init();
  lcd_brightness(0xFF);

  // Landscape mode
  lcd_setRotation(1);
  _width = TFT_HEIGHT; // 536
  _height = TFT_WIDTH; // 240

  // Clear to black
  lcd_fill(0, 0, _width, _height, 0x0000);

  Serial.println("RM67162Display::begin() complete");
}

void RM67162Display::setRotation(uint8_t r) {
  _rotation = r & 0x03;
  if (_rotation % 2 == 0) {
    _width = RM67162_PANEL_WIDTH;
    _height = RM67162_PANEL_HEIGHT;
  } else {
    _width = RM67162_PANEL_HEIGHT;
    _height = RM67162_PANEL_WIDTH;
  }
  lcd_setRotation(_rotation);
}

void RM67162Display::drawPixel(int16_t x, int16_t y, uint16_t color) {
  if (x < 0 || y < 0 || x >= _width || y >= _height) {
    return;
  }
  lcd_DrawPoint(x, y, color);
}

void RM67162Display::fillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                              uint16_t color) {
  if (w <= 0 || h <= 0) {
    return;
  }
  if (x >= _width || y >= _height) {
    return;
  }
  if (x + w - 1 >= _width) {
    w = _width - x;
  }
  if (y + h - 1 >= _height) {
    h = _height - y;
  }

  lcd_fill(x, y, x + w, y + h, color);
}

void RM67162Display::fillScreen(uint16_t color) {
  fillRect(0, 0, _width, _height, color);
}
