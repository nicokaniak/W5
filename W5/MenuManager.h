#ifndef MENUMANAGER_H
#define MENUMANAGER_H

#include <Arduino.h>
#include "ButtonManager.h"

enum AppMode {
  MODE_WATCH,
  MODE_MENU,
  MODE_STOPWATCH,
  MODE_WEATHER,
  MODE_CONFIG,
  MODE_MENU_STYLE,  // sub-screen of CONFIG: pick the rotary menu visual style
};

// ponytail: rotary menu visual styles. Add new styles here AND in
// MENU_STYLE_LABELS (MenuManager.cpp) AND implement a renderer in
// DisplayManager::drawMenu dispatch. Index stored in NVS as "mstyle".
enum MenuStyle {
  STYLE_HUD = 0,     // tactical compass: tick gauge + radar sweep + reticle
  STYLE_HOLO = 1,    // holographic ring: double ring + light beam + scanlines
  STYLE_DATA = 2,    // data stream: telemetry columns + lock-on bar + circuit trace
  STYLE_MINIMAL = 3, // minimal sci-fi: glow halo + marching ants + bracket labels
  MENU_STYLE_COUNT,
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

  // Config sub-menu
  static uint8_t   configItemCount();
  static const char* configItemLabel(uint8_t i);
  static uint8_t   configSelectedIndex();

  // Menu style picker (sub-screen of CONFIG)
  static MenuStyle menuStyle();                     // current persisted style
  static void      setMenuStyle(MenuStyle s);       // persist to NVS
  static MenuStyle menuStylePickerIndex();          // live picker cursor (not yet applied)
  static const char* menuStyleLabel(uint8_t i);     // "HUD", "HOLO", ...
  static uint8_t   menuStyleCount();

  // Scroll animation
  static bool    isAnimating();
  static int8_t  scrollDir();       // -1 = scrolling up, +1 = scrolling down, 0 = idle
  static float   animProgress();    // 0.0 to 1.0 (eased)
  static void    updateAnimation(); // call each loop tick; marks done when finished

private:
  static AppMode   _mode;
  static uint8_t   _selectedIndex;
  static bool      _dirty;
  static uint8_t   _configIndex;

  static bool      _animating;
  static int8_t    _scrollDir;
  static uint32_t  _animStartTime;
  static MenuStyle _menuStyle;          // persisted style, loaded at init
  static MenuStyle _stylePickerIndex;   // live cursor in MODE_MENU_STYLE
  static void      startScroll(int8_t dir);
  static void      runWifiPortal();
};

#endif
