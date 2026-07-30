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
  static bool      consumeDirty();
  static uint8_t   menuItemCount();
  static const char* menuItemLabel(uint8_t i);

  // Scroll animation
  static bool    isAnimating();
  static int8_t  scrollDir();       // -1 = scrolling up, +1 = scrolling down, 0 = idle
  static float   animProgress();    // 0.0 to 1.0 (eased)
  static void    updateAnimation(); // call each loop tick; marks done when finished

private:
  static AppMode   _mode;
  static uint8_t   _selectedIndex;
  static bool      _dirty;

  static bool      _animating;
  static int8_t    _scrollDir;
  static uint32_t  _animStartTime;
  static void      startScroll(int8_t dir);
};

#endif
