#ifndef DISPLAYMANAGER_H
#define DISPLAYMANAGER_H

#include <Adafruit_GFX.h>
#include <Arduino.h>
#include "MenuManager.h" // AppMode for drawTransition

class DisplayManager {
public:
  static void initDisplay();
  static void drawWatchFace(const String &timeStr);
  static void drawMenu(uint8_t selectedIndex, int8_t scrollDir = 0, float animProgress = 1.0f);
  static void drawStopwatch();
  static void drawWeatherScreen();
  static void drawWifiConnecting();
  static void drawConfigMenu(uint8_t selectedIndex);
  static void drawMenuStylePicker(uint8_t pickerIndex);
  static void drawWifiPortalScreen();
  static void drawWifiResultScreen(bool connected, const String &message);
  // ponytail: dive/zoom transition. Bracket contracts from fullscreen into the
  // selected menu item (phase 1), swaps to the target screen, then expands back
  // to fullscreen (phase 2). Reuses smoothstep progress from MenuManager.
  static void drawTransition(AppMode fromMode, AppMode toMode,
                             uint8_t selectedIndex, float progress);

private:
  static GFXcanvas16 *canvas;
  static void pushToDisplay();
};

#endif
