#ifndef DISPLAYMANAGER_H
#define DISPLAYMANAGER_H

#include <Adafruit_GFX.h>
#include <Arduino.h>

class DisplayManager {
public:
  static void initDisplay();
  static void drawWatchFace(const String &timeStr);
  static void drawMenu(uint8_t selectedIndex, int8_t scrollDir = 0, float animProgress = 1.0f);
  static void drawStopwatch();
  static void drawPomodoro();
  static void drawWeatherScreen();
  static void drawWifiConnecting();
  static void drawConfigMenu(uint8_t selectedIndex);
  static void drawMenuStylePicker(uint8_t pickerIndex);
  static void drawBrightnessPicker(uint8_t pickerIndex);
  static void drawWifiPortalScreen();
  static void drawWifiResultScreen(bool connected, const String &message);
  // ponytail: sci-fi blink-shrink-slide transition. Phase 1 (power-down): arc
  // shifts cyan→amber as it shrinks/slides left; label blinks 3x with chromatic
  // aberration (red+cyan ghosts). Phase 2 (signal acquire): target screen slides
  // in right→left via scanline reveal, then lock-on flash (white brackets) fades.
  // Reuses smoothstep progress from MenuManager (TRANSITION_DURATION_MS).
  static void drawTransition(uint8_t selectedIndex, float progress);

private:
  static GFXcanvas16 *canvas;
  static void pushToDisplay();
};

#endif
