#include "StopwatchManager.h"

StopwatchManager::State    StopwatchManager::_state         = IDLE;
uint32_t StopwatchManager::_segmentStart    = 0;
uint32_t StopwatchManager::_accumulated     = 0;
uint32_t StopwatchManager::_lastLapSplit    = 0;
uint32_t StopwatchManager::_lapBaseElapsed  = 0;
uint8_t  StopwatchManager::_lapCount        = 0;
bool     StopwatchManager::_dirty           = false;

void StopwatchManager::init() {
  _state = IDLE;
  _segmentStart = 0;
  _accumulated = 0;
  _lastLapSplit = 0;
  _lapBaseElapsed = 0;
  _lapCount = 0;
  _dirty = true;
}

bool StopwatchManager::isRunning() { return _state == RUNNING; }
bool StopwatchManager::isStopped() { return _state == STOPPED; }

uint32_t StopwatchManager::getElapsedMs() {
  if (_state == RUNNING) return _accumulated + (millis() - _segmentStart);
  return _accumulated;
}

uint8_t  StopwatchManager::getLapCount()  { return _lapCount; }
uint32_t StopwatchManager::getLastLapMs() { return _lastLapSplit; }

bool StopwatchManager::consumeDirty() {
  bool d = _dirty;
  _dirty = false;
  return d;
}

void StopwatchManager::handleEvent(ButtonEvent evt) {
  switch (evt) {
    case EVENT_TOP_CLICK:
      // Start (from idle/stopped) or stop (when running)
      if (_state == IDLE || _state == STOPPED) {
        _segmentStart = millis();
        _state = RUNNING;
        _dirty = true;
        Serial.println("SW: start/resume");
      } else if (_state == RUNNING) {
        _accumulated += millis() - _segmentStart;
        _state = STOPPED;
        _dirty = true;
        Serial.println("SW: stop");
      }
      break;

    case EVENT_BOTTOM_CLICK:
      if (_state == RUNNING) {
        // Lap — record split since prior lap, keep running
        uint32_t el = getElapsedMs();
        _lastLapSplit = el - _lapBaseElapsed;
        _lapBaseElapsed = el;
        _lapCount++;
        _dirty = true;
        Serial.printf("SW: lap %u = %lums\n", _lapCount, _lastLapSplit);
      } else if (_state == STOPPED) {
        // Reset to idle
        _accumulated = 0;
        _lapBaseElapsed = 0;
        _lastLapSplit = 0;
        _lapCount = 0;
        _state = IDLE;
        _dirty = true;
        Serial.println("SW: reset");
      }
      break;

    default: break;
  }
}
