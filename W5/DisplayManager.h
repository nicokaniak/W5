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

private:
  static GFXcanvas16 *canvas;
  static void pushToDisplay();
};

#endif
