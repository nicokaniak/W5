#include "PomodoroManager.h"

// ponytail: classic Pomodoro durations. Compile-time constants — no NVS, no
// runtime config. If the user wants adjustable durations, swap these for
// Preferences-backed getters like MenuManager does for brightness/style.
static const uint32_t WORK_MS       = 25UL * 60UL * 1000UL;  // 25 min
static const uint32_t SHORT_BREAK_MS = 5UL * 60UL * 1000UL;   // 5 min
static const uint32_t LONG_BREAK_MS  = 15UL * 60UL * 1000UL;  // 15 min
static const uint8_t  WORK_BEFORE_LONG = 4; // long break after 4 work sessions
// ponytail: blink for 10s when a phase ends. If the user doesn't press anything,
// auto-advance so the next phase starts without intervention.
static const uint32_t ALERT_DURATION_MS = 10000;

PomodoroManager::State    PomodoroManager::_state          = IDLE;
PomodoroPhase PomodoroManager::_phase          = PHASE_IDLE;
uint32_t      PomodoroManager::_phaseEnd       = 0;
uint32_t      PomodoroManager::_pausedRemaining = 0;
uint8_t       PomodoroManager::_workCount      = 0;
bool          PomodoroManager::_dirty          = false;
bool          PomodoroManager::_alerting       = false;
uint32_t      PomodoroManager::_alertStart     = 0;

void PomodoroManager::init() {
  _state = IDLE;
  _phase = PHASE_IDLE;
  _phaseEnd = 0;
  _pausedRemaining = 0;
  _workCount = 0;
  _alerting = false;
  _alertStart = 0;
  _dirty = true;
}

PomodoroPhase PomodoroManager::currentPhase() { return _phase; }
bool PomodoroManager::isRunning() { return _state == RUNNING; }
bool PomodoroManager::isPaused()  { return _state == PAUSED; }
bool PomodoroManager::isIdle()    { return _state == IDLE; }
bool PomodoroManager::isAlerting() { return _alerting; }

uint32_t PomodoroManager::phaseDurationMs(PomodoroPhase phase) {
  switch (phase) {
    case PHASE_WORK:        return WORK_MS;
    case PHASE_SHORT_BREAK: return SHORT_BREAK_MS;
    case PHASE_LONG_BREAK:  return LONG_BREAK_MS;
    default:                return 0;
  }
}

uint32_t PomodoroManager::getPhaseDurationMs() {
  return phaseDurationMs(_phase);
}

uint32_t PomodoroManager::getRemainingMs() {
  if (_state == RUNNING) {
    uint32_t rem = _phaseEnd - millis();
    return (rem > _phaseEnd) ? 0 : rem;  // underflow guard
  }
  if (_state == PAUSED) return _pausedRemaining;
  return phaseDurationMs(_phase);  // IDLE shows full duration
}

uint8_t PomodoroManager::getCompletedWorkCount() { return _workCount; }

bool PomodoroManager::consumeDirty() {
  bool d = _dirty;
  _dirty = false;
  return d;
}

void PomodoroManager::update() {
  if (_alerting) {
    if (millis() - _alertStart >= ALERT_DURATION_MS) {
      _alerting = false;
      advancePhase();
    }
    return;
  }
  if (_state != RUNNING) return;
  if (millis() >= _phaseEnd) {
    // ponytail: enter alerting state instead of immediately advancing — the
    // screen blinks to notify the user. Button press or timeout clears it.
    _alerting = true;
    _alertStart = millis();
    _state = IDLE;  // stop the countdown; phase info retained for display
    _dirty = true;
    Serial.println("POMO: phase ended — alerting");
  }
}

void PomodoroManager::startPhase(PomodoroPhase phase) {
  _phase = phase;
  _phaseEnd = millis() + phaseDurationMs(phase);
  _state = RUNNING;
  _dirty = true;
}

void PomodoroManager::advancePhase() {
  if (_phase == PHASE_WORK) {
    _workCount++;
    if (_workCount % WORK_BEFORE_LONG == 0) {
      startPhase(PHASE_LONG_BREAK);
      Serial.println("POMO: work done -> long break");
    } else {
      startPhase(PHASE_SHORT_BREAK);
      Serial.printf("POMO: work %u done -> short break\n", _workCount);
    }
  } else {
    // Break finished (or skipped) -> back to work
    if (_phase == PHASE_LONG_BREAK) {
      _workCount = 0;  // reset cycle after long break
      Serial.println("POMO: long break done -> new cycle");
    }
    startPhase(PHASE_WORK);
    Serial.println("POMO: break done -> work");
  }
}

void PomodoroManager::handleEvent(ButtonEvent evt) {
  // Any button press while alerting acknowledges it and advances.
  if (_alerting) {
    _alerting = false;
    advancePhase();
    Serial.println("POMO: alert acknowledged");
    return;
  }
  switch (evt) {
    case EVENT_TOP_CLICK:
      if (_state == IDLE) {
        startPhase(PHASE_WORK);
        Serial.println("POMO: start work");
      } else if (_state == RUNNING) {
        _pausedRemaining = _phaseEnd - millis();
        if (_pausedRemaining > _phaseEnd) _pausedRemaining = 0;  // underflow guard
        _state = PAUSED;
        _dirty = true;
        Serial.println("POMO: paused");
      } else if (_state == PAUSED) {
        _phaseEnd = millis() + _pausedRemaining;
        _state = RUNNING;
        _dirty = true;
        Serial.println("POMO: resumed");
      }
      break;

    case EVENT_BOTTOM_CLICK:
      // Skip current phase
      if (_state != IDLE) {
        advancePhase();
      }
      break;

    case EVENT_TOP_LONG_PRESS:
      // Reset to idle
      _state = IDLE;
      _phase = PHASE_IDLE;
      _phaseEnd = 0;
      _pausedRemaining = 0;
      _workCount = 0;
      _alerting = false;
      _dirty = true;
      Serial.println("POMO: reset");
      break;

    default: break;
  }
}
