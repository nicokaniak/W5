#include "BluetoothManager.h"
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>


// Nordic UART Service UUIDs
#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

static bool newNotification = false;
static String notificationMessage = "";
static bool deviceConnected = false;
static BLEServer *pServer = NULL;
static BLECharacteristic *pTxCharacteristic = NULL;

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;
    Serial.println("BLE Device Connected");
  };

  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
    Serial.println("BLE Device Disconnected");
    // Restart advertising
    pServer->getAdvertising()->start();
  }
};

class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    // Use Arduino String instead of std::string to avoid conversion errors
    String rxValue = pCharacteristic->getValue();

    if (rxValue.length() > 0) {
      notificationMessage = "";
      for (int i = 0; i < rxValue.length(); i++) {
        notificationMessage += rxValue[i];
      }
      newNotification = true;
      Serial.print("Received Value: ");
      Serial.println(notificationMessage);
    }
  }
};

void BluetoothManager::initBluetooth() {
  // Create the BLE Device
  BLEDevice::init("Lilygo_Watch");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic for TX (sending data)
  pTxCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
  pTxCharacteristic->addDescriptor(new BLE2902());

  // Create a BLE Characteristic for RX (receiving data)
  BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
  pRxCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(
      0x06); // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.println("BLE Started. Waiting for connections...");
}

void BluetoothManager::checkNotifications() {
  // Notifications are handled in the callback
}

bool BluetoothManager::hasNewNotification() { return newNotification; }

String BluetoothManager::getNotification() {
  newNotification = false;
  return notificationMessage;
}

bool BluetoothManager::isConnected() { return deviceConnected; }
