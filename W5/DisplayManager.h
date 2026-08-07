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
  static void drawColorSchemePicker();
  static void drawWifiPortalScreen();
  static void drawWifiResultScreen(bool connected, const String &message);
  // ponytail: sci-fi blink-shrink-slide transition (forward + reverse).
  // FORWARD (menu→screen): arc shrinks cyan→amber + label blinks/glitches +
  // underline retreats; then target screen slides in right→left via scanline
  // reveal + lock-on flash.
  // REVERSE (screen→menu): lock-off flash + current screen slides out right via
  // scanline de-materialize; then arc grows amber→cyan + label blinks/grows +
  // underline grows. Reuses smoothstep progress (TRANSITION_DURATION_MS).
  static void drawTransition(uint8_t selectedIndex, float progress);

private:
  static GFXcanvas16 *canvas;
  static void pushToDisplay();
};

#endif
