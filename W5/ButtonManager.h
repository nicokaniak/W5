#ifndef BUTTONMANAGER_H
#define BUTTONMANAGER_H

#include <Arduino.h>

// ponytail: click + long-press model (industry standard for two-button devices).
// No double-click — single clicks fire instantly with zero delay.
enum ButtonEvent {
  EVENT_NONE = 0,
  EVENT_TOP_CLICK,            // top button click         -> scroll up
  EVENT_BOTTOM_CLICK,         // bottom button click      -> scroll down
  EVENT_TOP_LONG_PRESS,       // top button hold ~600ms   -> select / enter
  EVENT_BOTTOM_LONG_PRESS,    // bottom button hold ~600ms -> go back
  EVENT_BOTH_PRESS,           // both pressed together    -> enter/exit menu
};

class ButtonManager {
public:
  static void init();
  static void update();             // call every loop iteration
  static ButtonEvent pollEvent();   // returns next event or EVENT_NONE

private:
  enum BtnState { IDLE, DOWN, LONG_FIRED };
  struct Btn {
    uint8_t  pin;
    BtnState state;
    bool     debounced;
    bool     lastRaw;
    uint32_t lastRawChange;
    uint32_t pressStart;
    bool     longFired;
  };
  static Btn _top;
  static Btn _bottom;
  static bool _bothFired;
  static ButtonEvent _pending;
  static void emit(ButtonEvent e);
  static void step(Btn &b, ButtonEvent clickEvt, ButtonEvent longEvt);
};

#endif
