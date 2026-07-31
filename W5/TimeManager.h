#ifndef TIMEMANAGER_H
#define TIMEMANAGER_H

#include <Arduino.h>

class TimeManager {
public:
  static void initTime();
  static void updateTime();
  static String getCurrentTime();
  static void drawWatchFace(const String &timeStr);
};

#endif
