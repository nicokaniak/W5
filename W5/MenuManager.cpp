#include "MenuManager.h"
#include "StopwatchManager.h"
#include "PomodoroManager.h"
#include "DisplayManager.h"
#include "TimeManager.h"
#include "WeatherManager.h"
#include <WiFi.h>
#include <WiFiManager.h>
#include <Preferences.h> // ESP32 NVS — in Arduino-ESP32 core, no external dep
#include "rm67162.h"     // lcd_brightness() — apply level to the panel

// ponytail: WiFiManager must be installed for on-device Wi-Fi setup. In Arduino IDE
// use the Library Manager; in PlatformIO add tzapu/WiFiManager to lib_deps.
static WiFiManager wifiManager;

// ponytail: items live here as the single source of truth. DisplayManager queries
// menuItemLabel(), the select handler maps index -> mode below. Keep both in sync.
static const char* MENU_ITEMS[] = { "Watch", "Stopwatch", "Pomodoro", "Weather", "Config" };
static const uint8_t NUM_ITEMS = 5;

// ponytail: config sub-menu. Adding items only requires extending this array;
// the handler dispatches by index below. Index 0 = Wi-Fi portal, 1 = style picker,
// 2 = brightness picker.
static const char* CONFIG_ITEMS[] = { "Setup Wi-Fi", "Menu Style", "Brightness" };
static const uint8_t NUM_CONFIG_ITEMS = 3;

// ponytail: rotary menu visual styles. Keep in sync with MenuStyle enum (header).
static const char* MENU_STYLE_LABELS[] = { "HUD", "HOLO", "DATA", "MINIMAL" };
static_assert(sizeof(MENU_STYLE_LABELS)/sizeof(MENU_STYLE_LABELS[0]) == MENU_STYLE_COUNT,
              "MENU_STYLE_LABELS must match MenuStyle enum");

// Compile-time guard: a config screen with zero items would be a broken state.
static_assert(NUM_CONFIG_ITEMS > 0, "config menu must have at least one item");

static const uint32_t ANIM_DURATION_MS = 350;  // scroll animation length

// ponytail: level 0..15 -> DBV 0..255 (level * 17). 17*15 = 255 exactly.
// File-local so DisplayManager's picker can re-derive the same fill count from
// the level without a circular include.
static inline uint8_t brightnessValue(uint8_t level) {
  if (level >= BRIGHTNESS_LEVELS) level = BRIGHTNESS_LEVELS - 1;
  return (uint8_t)(level * 17);
}

AppMode  MenuManager::_mode          = MODE_WATCH;
uint8_t  MenuManager::_selectedIndex = 0;
bool     MenuManager::_dirty         = false;
uint8_t  MenuManager::_configIndex   = 0;
bool     MenuManager::_animating     = false;
int8_t   MenuManager::_scrollDir     = 0;
uint32_t MenuManager::_animStartTime = 0;
MenuStyle MenuManager::_menuStyle       = STYLE_HUD;
MenuStyle MenuManager::_stylePickerIndex = STYLE_HUD;
uint8_t   MenuManager::_brightness       = BRIGHTNESS_LEVELS - 1; // default max
uint8_t   MenuManager::_brightnessPicker = BRIGHTNESS_LEVELS - 1;
AppMode   MenuManager::_pendingMode      = MODE_WATCH;

void MenuManager::init() {
  _mode = MODE_WATCH;
  _selectedIndex = 0;
  _dirty = false;
  _configIndex = 0;
  _animating = false;
  _scrollDir = 0;
  // Load persisted menu style from NVS. Default STYLE_HUD if unset/invalid.
  Preferences prefs;
  prefs.begin("w5", true); // read-only
  uint8_t s = prefs.getUChar("mstyle", (uint8_t)STYLE_HUD);
  uint8_t b = prefs.getUChar("bright", BRIGHTNESS_LEVELS - 1);
  prefs.end();
  if (s >= MENU_STYLE_COUNT) s = (uint8_t)STYLE_HUD;
  _menuStyle = (MenuStyle)s;
  _stylePickerIndex = _menuStyle;
  if (b >= BRIGHTNESS_LEVELS) b = BRIGHTNESS_LEVELS - 1;
  _brightness = b;
  _brightnessPicker = b;
  lcd_brightness(brightnessValue(b));  // apply persisted brightness to the panel
  _pendingMode = MODE_WATCH;
}

AppMode   MenuManager::currentMode()    { return _mode; }
uint8_t   MenuManager::selectedIndex()  { return _selectedIndex; }
AppMode   MenuManager::pendingMode()    { return _pendingMode; }

bool MenuManager::consumeDirty() {
  bool d = _dirty;
  _dirty = false;
  return d;
}

uint8_t MenuManager::menuItemCount() { return NUM_ITEMS; }

const char* MenuManager::menuItemLabel(uint8_t i) {
  if (i >= NUM_ITEMS) return "";
  return MENU_ITEMS[i];
}

uint8_t MenuManager::configItemCount() { return NUM_CONFIG_ITEMS; }

const char* MenuManager::configItemLabel(uint8_t i) {
  if (i >= NUM_CONFIG_ITEMS) return "";
  return CONFIG_ITEMS[i];
}

uint8_t MenuManager::configSelectedIndex() { return _configIndex; }

MenuStyle MenuManager::menuStyle()          { return _menuStyle; }
MenuStyle MenuManager::menuStylePickerIndex() { return _stylePickerIndex; }
uint8_t   MenuManager::menuStyleCount()     { return MENU_STYLE_COUNT; }
const char* MenuManager::menuStyleLabel(uint8_t i) {
  if (i >= MENU_STYLE_COUNT) return "";
  return MENU_STYLE_LABELS[i];
}

void MenuManager::setMenuStyle(MenuStyle s) {
  if ((uint8_t)s >= MENU_STYLE_COUNT) return;
  _menuStyle = s;
  Preferences prefs;
  prefs.begin("w5", false); // read-write
  prefs.putUChar("mstyle", (uint8_t)s);
  prefs.end();
  Serial.printf("MENU: style set -> %u (%s)\n", (uint8_t)s, MENU_STYLE_LABELS[s]);
}

uint8_t MenuManager::brightness()          { return _brightness; }
uint8_t MenuManager::brightnessPickerIndex() { return _brightnessPicker; }

void MenuManager::setBrightness(uint8_t level) {
  if (level >= BRIGHTNESS_LEVELS) level = BRIGHTNESS_LEVELS - 1;
  _brightness = level;
  lcd_brightness(brightnessValue(level));  // apply to panel immediately
  Preferences prefs;
  prefs.begin("w5", false); // read-write
  prefs.putUChar("bright", level);
  prefs.end();
  Serial.printf("BRIGHT: level %u -> DBV %u\n", level, brightnessValue(level));
}

bool MenuManager::isAnimating() { return _animating; }
int8_t MenuManager::scrollDir() { return _scrollDir; }

float MenuManager::animProgress() {
  if (!_animating) return 1.0f;
  float t = (float)(millis() - _animStartTime) / (float)ANIM_DURATION_MS;
  if (t >= 1.0f) t = 1.0f;
  // ponytail: smoothstep (t*t*(3-2*t)) — zero velocity at both endpoints.
  // Items start gently, accelerate through the middle, decelerate to a stop.
  // Feels like a physical carousel. Was ease-out (1-(1-t)^2) which jumped at t=0.
  return t * t * (3.0f - 2.0f * t);
}

void MenuManager::updateAnimation() {
  if (!_animating) return;
  if (millis() - _animStartTime >= ANIM_DURATION_MS) {
    _animating = false;
    _scrollDir = 0;
    // ponytail: if we were in a dive/zoom transition, resolve into the target screen.
    if (_mode == MODE_TRANSITION) {
      _mode = _pendingMode;
      Serial.printf("MENU: transition resolved -> mode %d\n", (int)_mode);
    }
    _dirty = true;  // one final clean redraw at the settled position
  }
}

void MenuManager::startScroll(int8_t dir) {
  _scrollDir = dir;
  _animStartTime = millis();
  _animating = true;
}

// ponytail: map a screen mode back to its top-level menu index, so exiting any
// screen lands on the menu option it was entered from (rather than always 0).
// Must stay in sync with the select handler's index -> _pendingMode mapping.
static uint8_t modeToMenuIndex(AppMode m) {
  switch (m) {
    case MODE_WATCH:     return 0;
    case MODE_STOPWATCH: return 1;
    case MODE_POMODORO:  return 2;
    case MODE_WEATHER:   return 3;
    case MODE_CONFIG:    return NUM_ITEMS - 1; // 4
    case MODE_TRANSITION: return modeToMenuIndex(MenuManager::pendingMode());
    default:            return 0;
  }
}

void MenuManager::handleEvent(ButtonEvent evt) {
  if (evt == EVENT_NONE) return;

  if (evt == EVENT_BOTH_PRESS) {
    _animating = false;
    _scrollDir = 0;
    if (_mode == MODE_MENU) {
      _mode = MODE_WATCH;
    } else if (_mode == MODE_MENU_STYLE) {
      // Exit style picker back to config sub-menu (discard unapplied cursor).
      _stylePickerIndex = _menuStyle;
      _mode = MODE_CONFIG;
      _configIndex = 1; // land back on "Menu Style"
    } else if (_mode == MODE_BRIGHTNESS) {
      // Exit brightness picker back to config sub-menu (discard unapplied cursor,
      // restore the persisted brightness so the panel reflects the saved state).
      _brightnessPicker = _brightness;
      lcd_brightness(brightnessValue(_brightness));
      _mode = MODE_CONFIG;
      _configIndex = 2; // land back on "Brightness"
    } else if (_mode == MODE_CONFIG) {
      // Exit config sub-menu back to the main Config item.
      _mode = MODE_MENU;
      _selectedIndex = NUM_ITEMS - 1;
    } else {
      // Exit any screen back to its own menu entry (not always index 0).
      _selectedIndex = modeToMenuIndex(_mode);
      _mode = MODE_MENU;
    }
    _dirty = true;
    Serial.printf("MENU: mode -> %d\n", (int)_mode);
    return;
  }

  // Long-press BOTTOM: go back to the menu from any screen, or exit menu to
  // watch. From the watch face, it enters the menu — so there's always an easy
  // way in without the simultaneous two-button press.
  if (evt == EVENT_BOTTOM_LONG_PRESS) {
    _animating = false;
    _scrollDir = 0;
    if (_mode == MODE_MENU) {
      _mode = MODE_WATCH;
    } else if (_mode == MODE_MENU_STYLE) {
      _stylePickerIndex = _menuStyle;  // discard unapplied cursor
      _mode = MODE_MENU;
      _selectedIndex = NUM_ITEMS - 1;  // land on Config item
    } else if (_mode == MODE_BRIGHTNESS) {
      _brightnessPicker = _brightness;  // discard unapplied cursor
      lcd_brightness(brightnessValue(_brightness));  // restore panel
      _mode = MODE_MENU;
      _selectedIndex = NUM_ITEMS - 1;
    } else {
      // From WATCH, STOPWATCH, POMODORO, WEATHER, TRANSITION -> go to menu,
      // landing on the entry the screen was opened from.
      _selectedIndex = modeToMenuIndex(_mode);
      _mode = MODE_MENU;
    }
    _dirty = true;
    Serial.printf("MENU: back -> mode %d\n", (int)_mode);
    return;
  }

  // Stopwatch handles its own button events (start/stop/lap/reset);
  // BOTH_PRESS and BOTTOM_LONG_PRESS already exited to menu above.
  if (_mode == MODE_STOPWATCH) {
    StopwatchManager::handleEvent(evt);
    return;
  }

  // Pomodoro handles its own button events (start/pause/skip/reset);
  // BOTH_PRESS and BOTTOM_LONG_PRESS already exited to menu above.
  if (_mode == MODE_POMODORO) {
    PomodoroManager::handleEvent(evt);
    return;
  }

  // Menu style picker sub-screen.
  if (_mode == MODE_MENU_STYLE) {
    switch (evt) {
      case EVENT_TOP_CLICK:
        _stylePickerIndex = (MenuStyle)((_stylePickerIndex + 1) % MENU_STYLE_COUNT);
        _dirty = true;
        Serial.printf("STYLE: cursor -> %u (%s)\n", (uint8_t)_stylePickerIndex,
                      MENU_STYLE_LABELS[_stylePickerIndex]);
        break;
      case EVENT_BOTTOM_CLICK:
        _stylePickerIndex = (MenuStyle)((_stylePickerIndex + MENU_STYLE_COUNT - 1) % MENU_STYLE_COUNT);
        _dirty = true;
        Serial.printf("STYLE: cursor -> %u (%s)\n", (uint8_t)_stylePickerIndex,
                      MENU_STYLE_LABELS[_stylePickerIndex]);
        break;
      case EVENT_TOP_LONG_PRESS:
        // Apply + exit to main menu so the user sees the new style live.
        setMenuStyle(_stylePickerIndex);
        _mode = MODE_MENU;
        _selectedIndex = NUM_ITEMS - 1; // land on Config item
        _dirty = true;
        Serial.printf("STYLE: applied %u, mode -> MENU\n", (uint8_t)_stylePickerIndex);
        break;
      default: break;
    }
    return;
  }

  // Brightness picker sub-screen.
  if (_mode == MODE_BRIGHTNESS) {
    switch (evt) {
      case EVENT_TOP_CLICK:
        _brightnessPicker = (_brightnessPicker + 1) % BRIGHTNESS_LEVELS;
        lcd_brightness(brightnessValue(_brightnessPicker)); // live preview
        _dirty = true;
        Serial.printf("BRIGHT: cursor -> %u (DBV %u)\n",
                      _brightnessPicker, brightnessValue(_brightnessPicker));
        break;
      case EVENT_BOTTOM_CLICK:
        _brightnessPicker = (_brightnessPicker + BRIGHTNESS_LEVELS - 1) % BRIGHTNESS_LEVELS;
        lcd_brightness(brightnessValue(_brightnessPicker)); // live preview
        _dirty = true;
        Serial.printf("BRIGHT: cursor -> %u (DBV %u)\n",
                      _brightnessPicker, brightnessValue(_brightnessPicker));
        break;
      case EVENT_TOP_LONG_PRESS:
        // Apply + exit to main menu so the user sees the new brightness live.
        setBrightness(_brightnessPicker);
        _mode = MODE_MENU;
        _selectedIndex = NUM_ITEMS - 1; // land on Config item
        _dirty = true;
        Serial.printf("BRIGHT: applied %u, mode -> MENU\n", _brightnessPicker);
        break;
      default: break;
    }
    return;
  }

  // Config sub-menu handling.
  if (_mode == MODE_CONFIG) {
    switch (evt) {
      case EVENT_TOP_CLICK:
        _configIndex = (_configIndex + 1) % NUM_CONFIG_ITEMS;
        _dirty = true;
        Serial.printf("CONFIG: scroll -> %u (%s)\n", _configIndex, CONFIG_ITEMS[_configIndex]);
        break;
      case EVENT_BOTTOM_CLICK:
        _configIndex = (_configIndex + NUM_CONFIG_ITEMS - 1) % NUM_CONFIG_ITEMS;
        _dirty = true;
        Serial.printf("CONFIG: scroll -> %u (%s)\n", _configIndex, CONFIG_ITEMS[_configIndex]);
        break;
      case EVENT_TOP_LONG_PRESS:
        if (_configIndex == 0) {
          runWifiPortal();
        } else if (_configIndex == 1) {
          // Open the menu style picker; cursor starts at the current style.
          _stylePickerIndex = _menuStyle;
          _mode = MODE_MENU_STYLE;
          _dirty = true;
          Serial.println("CONFIG: open style picker");
        } else if (_configIndex == 2) {
          // Open the brightness picker; cursor starts at the current level.
          _brightnessPicker = _brightness;
          _mode = MODE_BRIGHTNESS;
          _dirty = true;
          Serial.println("CONFIG: open brightness picker");
        }
        break;
      default: break;
    }
    return;
  }

  if (_mode != MODE_MENU) return;

  switch (evt) {
    case EVENT_TOP_CLICK:
      _selectedIndex = (_selectedIndex + 1) % NUM_ITEMS;
      startScroll(+1);  // scroll up — items slide UP, item below rises to center
      Serial.printf("MENU: scroll up -> %u (%s)\n", _selectedIndex, MENU_ITEMS[_selectedIndex]);
      break;
    case EVENT_BOTTOM_CLICK:
      _selectedIndex = (_selectedIndex + NUM_ITEMS - 1) % NUM_ITEMS;
      startScroll(-1);  // scroll down — items slide DOWN, item above drops to center
      Serial.printf("MENU: scroll down -> %u (%s)\n", _selectedIndex, MENU_ITEMS[_selectedIndex]);
      break;
    case EVENT_TOP_LONG_PRESS:
      _animating = false;
      _scrollDir = 0;
      // ponytail: dive/zoom transition — bracket contracts into the selected
      // item, swaps to the target screen, then expands back to fullscreen.
      switch (_selectedIndex) {
        case 0:  _pendingMode = MODE_WATCH;     break;
        case 1:  _pendingMode = MODE_STOPWATCH; break;
        case 2:  _pendingMode = MODE_POMODORO;  break;
        case 3:  _pendingMode = MODE_WEATHER;   break;
        case 4:  _pendingMode = MODE_CONFIG;    break;
        default: _pendingMode = MODE_WATCH;     break;
      }
      _mode = MODE_TRANSITION;
      startScroll(0);  // scrollDir=0: reuse the animation timer without menu scroll
      Serial.printf("MENU: select %u -> transition to mode %d\n",
                    _selectedIndex, (int)_pendingMode);
      break;
    default: break;
  }
}

void MenuManager::runWifiPortal() {
  // ponytail: blocking portal is the smallest implementation. The device becomes an
  // AP named "W5-Setup" and blocks here until the user submits credentials or the
  // portal times out. Buttons are not polled while the portal is open.
  DisplayManager::drawWifiPortalScreen();
  wifiManager.setDebugOutput(false);
  wifiManager.setConfigPortalTimeout(120);

  bool connected = wifiManager.startConfigPortal("W5-Setup");

  if (connected) {
    String ssid = WiFi.SSID();
    DisplayManager::drawWifiResultScreen(true, "Connected to " + ssid);
    delay(2000);

    // ponytail: re-sync time and weather immediately. Time sync can take up to ~15s
    // if NTP is slow; this is acceptable because the user is in a setup flow.
    WeatherManager::updateWeather();
    TimeManager::initTime();

    DisplayManager::drawWifiResultScreen(true, "Time & weather synced");
    delay(2000);
  } else {
    DisplayManager::drawWifiResultScreen(false, "Setup cancelled / timeout");
    delay(3000);
  }

  _mode = MODE_WATCH;
  _dirty = true;
}
