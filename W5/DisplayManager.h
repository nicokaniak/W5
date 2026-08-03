#ifndef DISPLAYMANAGER_H
#define DISPLAYMANAGER_H

#include <Adafruit_GFX.h>
#include <Arduino.h>

class DisplayManager {
public:
  static void initDisplay();
  static void clearDisplay();
  static void drawText(const String &text, int x, int y);
  static void drawWatchFace(const String &timeStr);
  static void drawMenu(uint8_t selectedIndex, int8_t scrollDir = 0, float animProgress = 1.0f);
  static void drawStopwatch();
  static void drawWeatherScreen();
  static void drawAlarmsScreen();
  static void drawBatteryScreen();
  static void drawBluetoothScreen();
  static void drawWifiConnecting();
  static void drawConfigMenu(uint8_t selectedIndex);
  static void drawMenuStylePicker(uint8_t pickerIndex);
  static void drawWifiPortalScreen();
  static void drawWifiResultScreen(bool connected, const String &message);

private:
  static GFXcanvas16 *canvas;
  static void pushToDisplay();
};

#endif
