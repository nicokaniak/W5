#ifndef ALARMMANAGER_H
#define ALARMMANAGER_H

#include <Arduino.h>

class AlarmManager {
public:
  static void initAlarms();
  static void checkAlarms();
  static void setAlarm(uint8_t hour, uint8_t minute);
  static bool isAlarmActive();
};

#endif
