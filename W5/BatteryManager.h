#ifndef BATTERYMANAGER_H
#define BATTERYMANAGER_H

#include <Arduino.h>

class BatteryManager {
public:
  static void initBattery();
  static float getVoltage();
  static int getPercentage();
};

#endif
