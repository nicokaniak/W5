#include "MenuManager.h"
#include "StopwatchManager.h"
#include "DisplayManager.h"
#include "TimeManager.h"
#include "WeatherManager.h"
#include <WiFi.h>

// ponytail: WiFiManager is an optional library; guard it so the sketch still builds
// if it isn't installed, but the menu item will report that it is missing.
#if __has_include(<WiFiManager.h>)
#include <WiFiManager.h>
#define HAS_WIFIMANAGER 1
static WiFiManager wifiManager;
#else
#define HAS_WIFIMANAGER 0
#endif

// ponytail: items live here as the single source of truth. DisplayManager queries
// menuItemLabel(), the select handler maps index -> mode below. Keep both in sync.
static const char* MENU_ITEMS[] = { "Watch", "Stopwatch", "Config" };
static const uint8_t NUM_ITEMS = 3;

// ponytail: config sub-menu. Currently one item; the UI and handler are sized for
// a small vertical list, but adding items only requires extending this array.
static const char* CONFIG_ITEMS[] = { "Setup Wi-Fi" };
static const uint8_t NUM_CONFIG_ITEMS = 1;

// Compile-time guard: a config screen with zero items would be a broken state.
static_assert(NUM_CONFIG_ITEMS > 0, "config menu must have at least one item");

static const uint32_t ANIM_DURATION_MS = 350;  // scroll animation length

AppMode  MenuManager::_mode          = MODE_WATCH;
uint8_t  MenuManager::_selectedIndex = 0;
bool     MenuManager::_dirty         = false;
uint8_t  MenuManager::_configIndex   = 0;
bool     MenuManager::_animating     = false;
int8_t   MenuManager::_scrollDir     = 0;
uint32_t MenuManager::_animStartTime = 0;

void MenuManager::init() {
  _mode = MODE_WATCH;
  _selectedIndex = 0;
  _dirty = false;
  _configIndex = 0;
  _animating = false;
  _scrollDir = 0;
}

AppMode   MenuManager::currentMode()    { return _mode; }
uint8_t   MenuManager::selectedIndex()  { return _selectedIndex; }

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
    _dirty = true;  // one final clean redraw at the settled position
  }
}

void MenuManager::startScroll(int8_t dir) {
  _scrollDir = dir;
  _animStartTime = millis();
  _animating = true;
}

void MenuManager::handleEvent(ButtonEvent evt) {
  if (evt == EVENT_NONE) return;

  if (evt == EVENT_BOTH_LONG_PRESS) {
    _animating = false;
    _scrollDir = 0;
    if (_mode == MODE_MENU) {
      _mode = MODE_WATCH;
    } else if (_mode == MODE_CONFIG) {
      // Exit config sub-menu back to the main Config item.
      _mode = MODE_MENU;
      _selectedIndex = NUM_ITEMS - 1;
    } else {
      _mode = MODE_MENU;
      _selectedIndex = 0;
    }
    _dirty = true;
    Serial.printf("MENU: mode -> %d\n", (int)_mode);
    return;
  }

  // Stopwatch handles its own button events (start/stop/lap/reset);
  // BOTH_LONG_PRESS already exited to menu above.
  if (_mode == MODE_STOPWATCH) {
    StopwatchManager::handleEvent(evt);
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
      case EVENT_TOP_DOUBLE_CLICK:
        runWifiPortal();
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
    case EVENT_TOP_DOUBLE_CLICK:
      _animating = false;
      _scrollDir = 0;
      switch (_selectedIndex) {
        case 0:  _mode = MODE_WATCH;     break;
        case 1:  _mode = MODE_STOPWATCH; break;
        case 2:  _mode = MODE_CONFIG;    break;
        default: _mode = MODE_WATCH;     break;
      }
      _dirty = true;
      Serial.printf("MENU: select %u -> mode %d\n", _selectedIndex, (int)_mode);
      break;
    default: break;
  }
}

void MenuManager::runWifiPortal() {
#if HAS_WIFIMANAGER
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
#else
  DisplayManager::drawWifiResultScreen(false, "WiFiManager not installed");
  delay(3000);
#endif

  _mode = MODE_WATCH;
  _dirty = true;
}
