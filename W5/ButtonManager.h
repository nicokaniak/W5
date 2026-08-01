#ifndef BUTTONMANAGER_H
#define BUTTONMANAGER_H

#include <Arduino.h>

enum ButtonEvent {
  EVENT_NONE = 0,
  EVENT_TOP_CLICK,            // top button single click    -> scroll up
  EVENT_BOTTOM_CLICK,         // bottom button single click -> scroll down
  EVENT_TOP_DOUBLE_CLICK,     // top button double click    -> select
  EVENT_BOTH_LONG_PRESS,      // both held ~0.5s            -> enter/exit menu
};

class ButtonManager {
public:
  static void init();
  static void update();             // call every loop iteration (~50Hz)
  static ButtonEvent pollEvent();   // returns next event or EVENT_NONE

private:
  enum BtnState { IDLE, DOWN, ARMED, DOWN2 };
  struct Btn {
    uint8_t  pin;
    BtnState state;
    bool     debounced;
    bool     lastRaw;
    uint32_t lastRawChange;
    uint32_t pressStart;
    uint32_t releaseTime;
    bool     longConsumed;
  };
  static Btn _top;
  static Btn _bottom;
  static bool _bothLongFired;
  static ButtonEvent _pending;
  static void emit(ButtonEvent e);
  static void step(Btn &b, ButtonEvent clickEvt, ButtonEvent doubleClickEvt);
};

#endif
