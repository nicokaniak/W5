#ifndef TIMEMANAGER_H
#define TIMEMANAGER_H

#include <Arduino.h>

class TimeManager {
public:
  static void initTime();
  static void updateTime();
  static String getCurrentTime();
  static void drawWatchFace(const String &timeStr);
  // Set the watch clock from the phone's local wall-clock.
  // Format: year, month(1-12), day, hour, minute, second (local time).
  static void setLocalTime(int year, int month, int day, int hour, int minute, int second);
};

#endif
