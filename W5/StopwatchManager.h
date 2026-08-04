#ifndef STOPWATCHMANAGER_H
#define STOPWATCHMANAGER_H

#include <Arduino.h>
#include "ButtonManager.h"

// Two-button stopwatch state machine.
//   TOP click        -> Start (idle/stopped) / Stop (running)
//   BOTTOM click     -> Lap (running; records split since prior lap) / Reset (stopped)
//   BOTTOM long-press -> exit to menu (handled by MenuManager)
//   BOTH press       -> exit to menu (handled by MenuManager)
class StopwatchManager {
public:
  static void init();
  static void handleEvent(ButtonEvent evt);

  static bool     isRunning();
  static bool     isStopped();      // paused (not idle, not running)
  static uint32_t getElapsedMs();   // exact current running time
  static uint8_t  getLapCount();
  static uint32_t getLastLapMs();   // most recent lap split
  static bool     consumeDirty();   // true if a redraw is needed (state/lap change)

private:
  enum State { IDLE, RUNNING, STOPPED };
  static State    _state;
  static uint32_t _segmentStart;    // millis() when current run segment began
  static uint32_t _accumulated;     // running ms before current segment (pause/resume)
  static uint32_t _lastLapSplit;    // ms of most recent lap split
  static uint32_t _lapBaseElapsed;  // elapsed at last lap capture (for next split)
  static uint8_t  _lapCount;
  static bool     _dirty;
};

#endif
