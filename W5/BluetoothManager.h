#ifndef BLUETOOTHMANAGER_H
#define BLUETOOTHMANAGER_H

#include <Arduino.h>

class BluetoothManager {
public:
  static void initBluetooth();
  static void checkNotifications();
  static bool hasNewNotification();
  static String getNotification();
};

#endif
