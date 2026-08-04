#include "ButtonManager.h"
#include "pins_config.h"

// ponytail: User tested on hardware — GPIO 21 is the physical TOP button,
// GPIO 0 (BOOT) is the physical BOTTOM button.
// TOP button does double-duty: scroll up (single click) + select (double-click).
// BOTTOM button: scroll down only.

static const uint32_t DEBOUNCE_MS        = 20;
static const uint32_t SIMULTANEOUS_MS    = 200;   // both pressed within this window = "at the same time"
static const uint32_t DOUBLE_CLICK_MS    = 450;   // window between taps of a double-tap
static const uint32_t SHORT_PRESS_MAX_MS = 700;   // press shorter than this is a click candidate

// ponytail: zero-initialized at file scope (state=0=IDLE since IDLE is the first
// enumerator). pin is set in init() — can't use IDLE here because BtnState is private.
ButtonManager::Btn ButtonManager::_top;
ButtonManager::Btn ButtonManager::_bottom;
bool ButtonManager::_bothLongFired = false;
ButtonEvent ButtonManager::_pending = EVENT_NONE;

void ButtonManager::emit(ButtonEvent e) {
  // ponytail: single-slot pending event. If two events fire in one update tick,
  // the second overwrites the first. Rare and harmless (user just presses again).
  // Ceiling: missed event on simultaneous transitions. Upgrade: small ring buffer.
  _pending = e;
}

void ButtonManager::init() {
  _top.pin    = PIN_BUTTON_2;  // GPIO 21 (user)  -> physical TOP    -> scroll up + select
  _bottom.pin = PIN_BUTTON_1;  // GPIO 0  (BOOT)  -> physical BOTTOM -> scroll down
  _top.state    = _bottom.state    = IDLE;
  _top.debounced = _bottom.debounced = false;
  _top.lastRaw  = _bottom.lastRaw  = false;
  _top.lastRawChange  = _bottom.lastRawChange  = 0;
  _top.pressStart    = _bottom.pressStart    = 0;
  _top.releaseTime   = _bottom.releaseTime   = 0;
  _top.longConsumed  = _bottom.longConsumed  = false;
  _bothLongFired = false;
  _pending = EVENT_NONE;
  pinMode(_top.pin, INPUT_PULLUP);
  pinMode(_bottom.pin, INPUT_PULLUP);
}

void ButtonManager::step(Btn &b, ButtonEvent clickEvt, ButtonEvent doubleClickEvt) {
  uint32_t now = millis();

  // ARMED timeout: no second click came within the double-click window
  if (b.state == ARMED && now - b.releaseTime > DOUBLE_CLICK_MS) {
    emit(clickEvt);
    b.state = IDLE;
  }

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
        b.longConsumed = false;
      }
      break;
    case DOWN:
      if (!pressed) {
        if (b.longConsumed) {
          b.state = IDLE;  // was absorbed by both-long-press
        } else if (now - b.pressStart < SHORT_PRESS_MAX_MS) {
          b.state = ARMED;
          b.releaseTime = now;
        } else {
          b.state = IDLE;  // long single-button press, unused
        }
      }
      break;
    case ARMED:
      if (pressed) {
        if (now - b.releaseTime < DOUBLE_CLICK_MS) {
          // ponytail: fire on second tap (press down), not on release — feels like
          // a real double-tap instead of "press and hold the second time".
          emit(doubleClickEvt);
          b.state = DOWN2;
          b.pressStart = now;
          b.longConsumed = true;  // suppress emit on release
        } else {
          // window expired, emit click and start new press
          emit(clickEvt);
          b.state = DOWN;
          b.pressStart = now;
          b.longConsumed = false;
        }
      }
      break;
    case DOWN2:
      if (!pressed) {
        if (b.longConsumed) {
          b.state = IDLE;
        } else {
          emit(doubleClickEvt);
          b.state = IDLE;
        }
      }
      break;
  }
}

void ButtonManager::update() {
  // TOP button: scroll up (single click) + select (double-click)
  step(_top,    EVENT_TOP_CLICK,       EVENT_TOP_DOUBLE_CLICK);
  // BOTTOM button: scroll down only (no double-click action)
  step(_bottom, EVENT_BOTTOM_CLICK,    EVENT_NONE);

  // Both-press: fire as soon as both are held, no hold duration.
  // Require near-simultaneous press (within SIMULTANEOUS_MS) so scrolling with
  // one button then accidentally pressing the other doesn't trigger menu entry.
  bool topHeld    = (_top.state    == DOWN || _top.state    == DOWN2);
  bool bottomHeld = (_bottom.state == DOWN || _bottom.state == DOWN2);
  if (topHeld && bottomHeld && !_bothLongFired) {
    uint32_t earlier = min(_top.pressStart, _bottom.pressStart);
    uint32_t later   = max(_top.pressStart, _bottom.pressStart);
    if (later - earlier <= SIMULTANEOUS_MS) {
      emit(EVENT_BOTH_LONG_PRESS);
      _bothLongFired = true;
      _top.longConsumed    = true;  // suppress click/double-click on their release
      _bottom.longConsumed = true;
    }
  }
  if (!topHeld || !bottomHeld) {
    _bothLongFired = false;
  }
}

ButtonEvent ButtonManager::pollEvent() {
  ButtonEvent e = _pending;
  _pending = EVENT_NONE;
  return e;
}
