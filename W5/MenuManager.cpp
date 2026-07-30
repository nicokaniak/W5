#include "MenuManager.h"

// ponytail: items live here as the single source of truth. DisplayManager queries
// menuItemLabel(), the select handler maps index -> mode below. Keep both in sync.
static const char* MENU_ITEMS[] = { "Watch", "Stopwatch", "Configuration" };
static const uint8_t NUM_ITEMS = 3;

AppMode  MenuManager::_mode          = MODE_WATCH;
uint8_t  MenuManager::_selectedIndex = 0;
bool     MenuManager::_dirty         = false;

void MenuManager::init() {
  _mode = MODE_WATCH;
  _selectedIndex = 0;
  _dirty = false;
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

void MenuManager::handleEvent(ButtonEvent evt) {
  if (evt == EVENT_NONE) return;

  // ponytail: both-long-press is the universal menu toggle.
  //   In a feature mode -> enter MENU. In MENU -> cancel back to WATCH.
  if (evt == EVENT_BOTH_LONG_PRESS) {
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

  if (_mode != MODE_MENU) return;  // other events only matter inside the menu

  switch (evt) {
    case EVENT_TOP_CLICK:
      _selectedIndex = (_selectedIndex + NUM_ITEMS - 1) % NUM_ITEMS;  // scroll up
      _dirty = true;
      Serial.printf("MENU: scroll up -> %u (%s)\n", _selectedIndex, MENU_ITEMS[_selectedIndex]);
      break;
    case EVENT_BOTTOM_CLICK:
      _selectedIndex = (_selectedIndex + 1) % NUM_ITEMS;  // scroll down
      _dirty = true;
      Serial.printf("MENU: scroll down -> %u (%s)\n", _selectedIndex, MENU_ITEMS[_selectedIndex]);
      break;
    case EVENT_TOP_DOUBLE_CLICK:
      // ponytail: index mapping must match MENU_ITEMS above
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
