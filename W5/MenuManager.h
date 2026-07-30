#ifndef MENUMANAGER_H
#define MENUMANAGER_H

#include <Arduino.h>
#include "ButtonManager.h"

enum AppMode {
  MODE_WATCH,
  MODE_MENU,
  MODE_STOPWATCH,
  MODE_CONFIG,
};

class MenuManager {
public:
  static void init();
  static void handleEvent(ButtonEvent evt);
  static AppMode   currentMode();
  static uint8_t   selectedIndex();
  static bool      consumeDirty();     // true once after a state change, then clears
  static uint8_t   menuItemCount();
  static const char* menuItemLabel(uint8_t i);

private:
  static AppMode   _mode;
  static uint8_t   _selectedIndex;
  static bool      _dirty;
};

#endif
