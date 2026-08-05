#ifndef POMODOROMANAGER_H
#define POMODOROMANAGER_H

#include <Arduino.h>
#include "ButtonManager.h"

enum PomodoroPhase {
  PHASE_IDLE,
  PHASE_WORK,
  PHASE_SHORT_BREAK,
  PHASE_LONG_BREAK,
};

// Two-button Pomodoro timer state machine.
//   TOP click         -> Start (idle) / Pause (running) / Resume (paused)
//   BOTTOM click      -> Skip current phase (advance to next)
//   TOP long-press    -> Reset to idle
//   BOTTOM long-press -> exit to menu (handled by MenuManager)
//   BOTH press        -> exit to menu (handled by MenuManager)
//
// ponytail: classic Pomodoro cycle — 25 min work, 5 min short break, every 4th
// break is 15 min long break. Durations are compile-time constants; if runtime
// configurability is needed, swap to NVS-persisted values like brightness/style.
class PomodoroManager {
public:
  static void init();
  static void handleEvent(ButtonEvent evt);
  static void update();             // call each loop tick; auto-advances on phase end

  static PomodoroPhase currentPhase();
  static bool     isRunning();
  static bool     isPaused();
  static bool     isIdle();
  static uint32_t getRemainingMs();    // countdown remaining in current phase
  static uint32_t getPhaseDurationMs(); // total duration of current phase
  static uint8_t  getCompletedWorkCount(); // completed work sessions in current cycle
  static bool     consumeDirty();

private:
  enum State { IDLE, RUNNING, PAUSED };
  static State    _state;
  static PomodoroPhase _phase;
  static uint32_t _phaseEnd;         // millis() target when current phase ends
  static uint32_t _pausedRemaining;  // ms remaining when paused
  static uint8_t  _workCount;        // completed work sessions (resets at long break)
  static bool     _dirty;

  static void startPhase(PomodoroPhase phase);
  static void advancePhase();        // move to next phase in the cycle
  static uint32_t phaseDurationMs(PomodoroPhase phase);
};

#endif
