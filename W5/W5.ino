#include "AlarmManager.h"
#include "BatteryManager.h"
#include "BluetoothManager.h"
#include "DisplayManager.h"
#include "TimeManager.h"
#include "WeatherManager.h"
#include "config/config.h"
#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  delay(1000); // Wait for serial to initialize
  Serial.println("=== W5 Starting ===");
  Serial.println("Initializing display...");
  DisplayManager::initDisplay();
  Serial.println("Display initialized");

  // Initialize battery monitoring
  BatteryManager::initBattery();

  // Initialize WiFi first so NTP can sync
  WeatherManager::initWeather();

  TimeManager::initTime();
  AlarmManager::initAlarms();
  BluetoothManager::initBluetooth();
}

void loop() {
  TimeManager::updateTime();
  AlarmManager::checkAlarms();
  BluetoothManager::checkNotifications();
  WeatherManager::updateWeather();

  // Read battery status and print
  float batVolt = BatteryManager::getVoltage();
  int batPct = BatteryManager::getPercentage();
  Serial.print("Battery: ");
  Serial.print(batVolt, 2);
  Serial.print("V (");
  Serial.print(batPct);
  Serial.println("%)");

  DisplayManager::drawWatchFace(TimeManager::getCurrentTime());

  delay(1000); // update every second
}