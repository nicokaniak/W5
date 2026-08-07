#ifndef MENUMANAGER_H
#define MENUMANAGER_H

#include <Arduino.h>
#include "ButtonManager.h"
#include "ColorScheme.h"

enum AppMode {
  MODE_WATCH,
  MODE_MENU,
  MODE_STOPWATCH,
  MODE_POMODORO,
  MODE_WEATHER,
  MODE_CONFIG,
  MODE_MENU_STYLE,   // sub-screen of CONFIG: pick the rotary menu visual style
  MODE_BRIGHTNESS,   // sub-screen of CONFIG: adjust screen brightness
  MODE_COLOR_SCHEME, // sub-screen of CONFIG: pick main color and color-theory scheme
  MODE_TRANSITION,   // ponytail: dive/zoom animation between menu and selected screen
};

// ponytail: 16 discrete brightness levels map linearly to the RM67162's 0..255
// DBV range (level * 17 -> 0,17,...,255). 16 is enough granularity for a small
// AMOLED and keeps the picker UI as a simple 16-segment bar. Ceiling: the panel
// response is non-linear below ~30 DBV; if perceptual uniformity is needed,
// swap the linear map for a gamma LUT without touching the picker.
static const uint8_t BRIGHTNESS_LEVELS = 16;

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

  // Brightness picker (sub-screen of CONFIG)
  static uint8_t   brightness();                    // current persisted level (0..15)
  static void      setBrightness(uint8_t level);    // persist to NVS + apply to panel
  static uint8_t   brightnessPickerIndex();         // live picker cursor (not yet applied)

  // Color scheme picker (sub-screen of CONFIG)
  static ColorSchemeType colorSchemePickerType();   // live picker type (not yet applied)
  static ColorHSV        colorSchemePickerHSV();    // live picker color (not yet applied)
  static bool            colorSchemePickingHue();   // true = hue wheel, false = scheme list

  // Scroll animation
  static bool    isAnimating();
  static int8_t  scrollDir();       // -1 = scrolling up, +1 = scrolling down, 0 = idle
  static float   animProgress();    // 0.0 to 1.0 (eased)
  static void    updateAnimation(); // call each loop tick; marks done when finished

  // Dive/zoom transition (menu -> selected screen)
  static AppMode pendingMode();     // target screen the transition resolves into
  // Reverse transition (screen -> menu): slide current screen out, materialize menu
  static bool   transitionReverse();   // true during a screen→menu transition
  static AppMode transitionFromMode(); // screen we're leaving in a reverse transition

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
  static uint8_t   _brightness;         // persisted level (0..15), loaded at init
  static uint8_t   _brightnessPicker;   // live cursor in MODE_BRIGHTNESS
  static ColorSchemeType _colorSchemeTypePicker; // live cursor in MODE_COLOR_SCHEME
  static ColorHSV        _colorHSVPicker;        // live color in MODE_COLOR_SCHEME
  static bool            _colorSchemePickingHue; // list vs hue wheel
  static AppMode   _pendingMode;        // target screen during MODE_TRANSITION
  static bool      _transitionReverse;  // true = screen→menu, false = menu→screen
  static AppMode   _transitionFromMode; // screen being left in reverse transition
  static void      startScroll(int8_t dir);
  static void      runWifiPortal();
};

#endif
