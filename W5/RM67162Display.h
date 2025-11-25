#ifndef RM67162DISPLAY_H
#define RM67162DISPLAY_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "pins_config.h"

// Pin mappings for the LilyGO T-Display S3 AMOLED (RM67162 panel)
#ifndef RM67162_TFT_MOSI
#define RM67162_TFT_MOSI 18
#endif
#ifndef RM67162_TFT_SCK
#define RM67162_TFT_SCK 47
#endif
#ifndef RM67162_TFT_CS
#define RM67162_TFT_CS 6
#endif
#ifndef RM67162_TFT_DC
#define RM67162_TFT_DC 7
#endif
#ifndef RM67162_TFT_RST
#define RM67162_TFT_RST 17
#endif
#ifndef RM67162_TFT_BL
#define RM67162_TFT_BL 38
#endif

constexpr uint16_t RM67162_PANEL_WIDTH = TFT_WIDTH;
constexpr uint16_t RM67162_PANEL_HEIGHT = TFT_HEIGHT;

class RM67162Display : public Adafruit_GFX {
public:
  RM67162Display();
  void begin();
  void setRotation(uint8_t r) override;
  void drawPixel(int16_t x, int16_t y, uint16_t color) override;
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                uint16_t color) override;
  void fillScreen(uint16_t color) override;
private:
  uint8_t _rotation = 0;
};

#endif // RM67162DISPLAY_H
