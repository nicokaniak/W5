#include "BluetoothManager.h"

#if __has_include("sdkconfig.h")
#include "sdkconfig.h"
#endif

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_BT_ENABLED) && defined(CONFIG_BLUEDROID_ENABLED)
#include <BluetoothSerial.h>
#define HAS_CLASSIC_BT 1
static BluetoothSerial SerialBT;
#else
#define HAS_CLASSIC_BT 0
#endif

static bool newNotification = false;
static String notificationMessage = "";
static bool bluetoothReady = false;

void BluetoothManager::initBluetooth() {
#if HAS_CLASSIC_BT
  bluetoothReady = SerialBT.begin("Lilygo_Watch");
  if(!bluetoothReady) {
    Serial.println("Bluetooth initialization failed!");
  } else {
    Serial.println("Bluetooth started");
  }
#else
  Serial.println("Bluetooth disabled or unsupported in this build.");
#endif
}

void BluetoothManager::checkNotifications() {
  if(!bluetoothReady) {
    return;
  }

#if HAS_CLASSIC_BT
  if (SerialBT.available()) {
    notificationMessage = SerialBT.readStringUntil('\n');
    newNotification = true;
    Serial.println("New notification: " + notificationMessage);
  }
#endif
}

bool BluetoothManager::hasNewNotification() {
  return newNotification;
}

String BluetoothManager::getNotification() {
  newNotification = false;
  return notificationMessage;
}
