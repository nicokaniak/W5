#include "BluetoothManager.h"

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

#if HAS_BLE
static BLEServer* pServer = nullptr;
static BLECharacteristic* pTxCharacteristic = nullptr;

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* server) override {
    deviceConnected = true;
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
    if (value.length() > 0) {
      notificationMessage = value;
      newNotification = true;
      Serial.println("BLE notification: " + value);
    }
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
  // BLE data arrives via onWrite callback; nothing to poll.
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
