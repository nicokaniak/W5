#include "ButtonManager.h"
#include "pins_config.h"

// ponytail: User tested on hardware — GPIO 21 is the physical TOP button,
// GPIO 0 (BOOT) is the physical BOTTOM button.
// TOP:    click = scroll up,  long press = select/enter
// BOTTOM: click = scroll down, long press = go back
// BOTH:   pressed together = enter/exit menu

static const uint32_t DEBOUNCE_MS     = 20;
static const uint32_t LONG_PRESS_MS   = 300;   // hold threshold for long press
static const uint32_t SIMULTANEOUS_MS = 500;   // both pressed within this window = "together"

ButtonManager::Btn ButtonManager::_top;
ButtonManager::Btn ButtonManager::_bottom;
bool ButtonManager::_bothFired = false;
ButtonEvent ButtonManager::_pending = EVENT_NONE;

void ButtonManager::emit(ButtonEvent e) {
  _pending = e;
}

void ButtonManager::init() {
  _top.pin    = PIN_BUTTON_2;  // GPIO 21 -> physical TOP
  _bottom.pin = PIN_BUTTON_1;  // GPIO 0  -> physical BOTTOM
  _top.state    = _bottom.state    = IDLE;
  _top.debounced = _bottom.debounced = false;
  _top.lastRaw  = _bottom.lastRaw  = false;
  _top.lastRawChange  = _bottom.lastRawChange  = 0;
  _top.pressStart    = _bottom.pressStart    = 0;
  _top.longFired  = _bottom.longFired  = false;
  _bothFired = false;
  _pending = EVENT_NONE;
  pinMode(_top.pin, INPUT_PULLUP);
  pinMode(_bottom.pin, INPUT_PULLUP);
}

void ButtonManager::step(Btn &b, ButtonEvent clickEvt, ButtonEvent longEvt) {
  uint32_t now = millis();

  // Debounce raw read
  bool raw = (digitalRead(b.pin) == LOW);  // active low
  if (raw != b.lastRaw) {
    b.lastRaw = raw;
    b.lastRawChange = now;
  }
  if (now - b.lastRawChange >= DEBOUNCE_MS) {
    b.debounced = raw;
  }
  bool pressed = b.debounced;

  switch (b.state) {
    case IDLE:
      if (pressed) {
        b.state = DOWN;
        b.pressStart = now;
        b.longFired = false;
      }
      break;

    case DOWN:
      // Check for long press while held
      if (pressed && !b.longFired && (now - b.pressStart >= LONG_PRESS_MS)) {
        emit(longEvt);
        b.longFired = true;
        b.state = LONG_FIRED;
      }
      // Released before long threshold -> click
      if (!pressed && !b.longFired) {
        emit(clickEvt);
        b.state = IDLE;
      }
      break;

    case LONG_FIRED:
      // Wait for release; event already fired
      if (!pressed) {
        b.state = IDLE;
      }
      break;
  }
}

void ButtonManager::update() {
  step(_top,    EVENT_TOP_CLICK,    EVENT_TOP_LONG_PRESS);
  step(_bottom, EVENT_BOTTOM_CLICK, EVENT_BOTTOM_LONG_PRESS);

  // Both-press: both held and pressed within SIMULTANEOUS_MS of each other.
  // Fire as soon as the later one crosses the debounce (no hold duration).
  bool topDown    = (_top.state    == DOWN);
  bool bottomDown = (_bottom.state == DOWN);
  if (topDown && bottomDown && !_bothFired) {
    uint32_t earlier = min(_top.pressStart, _bottom.pressStart);
    uint32_t later   = max(_top.pressStart, _bottom.pressStart);
    if (later - earlier <= SIMULTANEOUS_MS) {
      emit(EVENT_BOTH_PRESS);
      _bothFired = true;
      _top.longFired    = true;  // suppress long press / click on release
      _bottom.longFired = true;
      _top.state    = LONG_FIRED;
      _bottom.state = LONG_FIRED;
    }
  }
  if (!topDown && !bottomDown) {
    _bothFired = false;
  }
}

ButtonEvent ButtonManager::pollEvent() {
  ButtonEvent e = _pending;
  _pending = EVENT_NONE;
  return e;
}
