#ifndef DISPLAYMANAGER_H
#define DISPLAYMANAGER_H

#include <Arduino.h>

class DisplayManager {
public:
  static void initDisplay();
  static void clearDisplay();
  static void drawText(const String &text, int x, int y);
  static void drawWatchFace(const String &timeStr);
};

#endif
