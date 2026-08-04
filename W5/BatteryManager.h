#ifndef BATTERYMANAGER_H
#define BATTERYMANAGER_H

#include <Arduino.h>

class BatteryManager {
public:
  static float getVoltage();
  static int getPercentage();
  static bool isUsbPowerConnected();
};

#endif
