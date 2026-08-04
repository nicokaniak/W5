#ifndef BLUETOOTHMANAGER_H
#define BLUETOOTHMANAGER_H

#include <Arduino.h>

class BluetoothManager {
public:
  static void initBluetooth();
  static String getNotification();
  static bool isConnected();
};

#endif
