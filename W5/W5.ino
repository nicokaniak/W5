#include <Arduino.h>
#include "config/config.h"
#include "TimeManager.h"
#include "AlarmManager.h"
#include "BluetoothManager.h"
#include "WeatherManager.h"
#include "DisplayManager.h"

void setup() {
  Serial.begin(115200);
  DisplayManager::initDisplay();

  TimeManager::initTime();
  AlarmManager::initAlarms();
  BluetoothManager::initBluetooth();
  WeatherManager::initWeather();
}

void loop() {
  TimeManager::updateTime();
  AlarmManager::checkAlarms();
  BluetoothManager::checkNotifications();
  WeatherManager::updateWeather();

  DisplayManager::drawWatchFace(TimeManager::getCurrentTime());

  delay(1000); // update every second
}