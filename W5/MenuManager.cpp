#include "MenuManager.h"
#include "StopwatchManager.h"

// ponytail: items live here as the single source of truth. DisplayManager queries
// menuItemLabel(), the select handler maps index -> mode below. Keep both in sync.
static const char* MENU_ITEMS[] = { "Watch", "Stopwatch", "Config" };
static const uint8_t NUM_ITEMS = 3;

static const uint32_t ANIM_DURATION_MS = 350;  // scroll animation length

AppMode  MenuManager::_mode          = MODE_WATCH;
uint8_t  MenuManager::_selectedIndex = 0;
bool     MenuManager::_dirty         = false;
bool     MenuManager::_animating     = false;
int8_t   MenuManager::_scrollDir     = 0;
uint32_t MenuManager::_animStartTime = 0;

void MenuManager::init() {
  _mode = MODE_WATCH;
  _selectedIndex = 0;
  _dirty = false;
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
