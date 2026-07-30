#include "BluetoothManager.h"
#include "TimeManager.h"

#if __has_include("sdkconfig.h")
#include "sdkconfig.h"
#endif

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_BT_ENABLED)
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#define HAS_BLE 1
#else
#define HAS_BLE 0
#endif

// Nordic UART Service UUIDs — works with BLE serial apps (e.g. Serial Bluetooth Terminal)
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

static volatile bool newNotification = false;
static String notificationMessage = "";
static volatile bool deviceConnected = false;

// ponytail: "every 15 min" under manual BLE SCANNER can only mean the watch
// prompts the user; the phone can't be forced to push. Ceiling: prompt only
// fires while in MODE_WATCH (checkNotifications is called there). Upgrade
// path: a Tasker/MacroDroid profile or custom app auto-writing SETTIME.
static const uint32_t TIME_PROMPT_MS = 15UL * 60UL * 1000UL; // 15 min
static uint32_t lastTimePrompt = 0;
static const char TIME_PROMPT[] = "Send: SETTIME:YYYY-MM-DD HH:MM:SS\r\n";

#if HAS_BLE
static BLEServer* pServer = nullptr;
static BLECharacteristic* pTxCharacteristic = nullptr;

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* server) override {
    deviceConnected = true;
    lastTimePrompt = millis(); // first periodic prompt 15 min from now
    // Nudge the user to send the time immediately on connect.
    if (pTxCharacteristic) {
      pTxCharacteristic->setValue(TIME_PROMPT);
      pTxCharacteristic->notify();
    }
    Serial.println("BLE client connected");
  }
  void onDisconnect(BLEServer* server) override {
    deviceConnected = false;
    Serial.println("BLE client disconnected, restarting advertising");
    BLEDevice::startAdvertising();
  }
};

class RxCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) override {
    String value = pCharacteristic->getValue().c_str();
    if (value.length() == 0) return;

    if (value.startsWith("SETTIME:")) {
      // Expected: SETTIME:YYYY-MM-DD HH:MM:SS  (phone local time, 24h)
      int y, mo, d, h, mi, s;
      if (sscanf(value.c_str() + 8, "%d-%d-%d %d:%d:%d",
                 &y, &mo, &d, &h, &mi, &s) == 6) {
        TimeManager::setLocalTime(y, mo, d, h, mi, s);
      } else {
        Serial.println("BLE: bad SETTIME format, use SETTIME:YYYY-MM-DD HH:MM:SS");
      }
      return;
    }

    notificationMessage = value;
    newNotification = true;
    Serial.println("BLE notification: " + value);
  }
};
#endif

void BluetoothManager::initBluetooth() {
#if HAS_BLE
  BLEDevice::init("Lilygo_Watch");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEService* pService = pServer->createService(SERVICE_UUID);

  pTxCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID_TX,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic* pRxCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID_RX,
    BLECharacteristic::PROPERTY_WRITE
  );
  pRxCharacteristic->setCallbacks(new RxCallbacks());

  pService->start();

  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.println("BLE started, advertising as \"Lilygo_Watch\"");
#else
  Serial.println("BLE not available in this build.");
#endif
}

void BluetoothManager::checkNotifications() {
  // BLE data arrives via onWrite callback; nothing to poll for notifications.
  // But we use this 1Hz tick (called from MODE_WATCH) to nudge the phone user
  // to re-send the time every 15 min.
#if HAS_BLE
  if (!deviceConnected) return;
  uint32_t now = millis();
  if (lastTimePrompt == 0 || now - lastTimePrompt >= TIME_PROMPT_MS) {
    if (pTxCharacteristic) {
      pTxCharacteristic->setValue(TIME_PROMPT);
      pTxCharacteristic->notify();
    }
    lastTimePrompt = now;
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

bool BluetoothManager::isConnected() {
  return deviceConnected;
}
