#include "AlarmManager.h"
#include "TimeManager.h"
#include <Arduino.h>

static uint8_t alarmHour = 0;
static uint8_t alarmMinute = 0;
static bool alarmSet = false;
static bool alarmActive = false;

void AlarmManager::initAlarms() {
  alarmSet = false;
  alarmActive = false;
}

void AlarmManager::setAlarm(uint8_t hour, uint8_t minute) {
  alarmHour = hour;
  alarmMinute = minute;
  alarmSet = true;
  alarmActive = false;
}

void AlarmManager::checkAlarms() {
  if (!alarmSet)
    return;

  String currentTime = TimeManager::getCurrentTime();
  int currentHour = currentTime.substring(0, 2).toInt();
  int currentMinute = currentTime.substring(3, 5).toInt();

  if (currentHour == alarmHour && currentMinute == alarmMinute &&
      !alarmActive) {
    alarmActive = true;
    // Trigger alarm action here (buzz, display, etc.)
    Serial.println("Alarm Triggered!");
  }
}

bool AlarmManager::isAlarmActive() { return alarmActive; }
