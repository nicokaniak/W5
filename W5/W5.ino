#include "AlarmManager.h"
#include "BatteryManager.h"
#include "BluetoothManager.h"
#include "DisplayManager.h"
#include "TimeManager.h"
#include "WeatherManager.h"
#include "config/config.h"
#include <Arduino.h>

void setup() {
  // ===== CRITICAL: ENABLE POWER FIRST - BEFORE ANYTHING ELSE =====
  // This MUST be the very first thing, even before Serial.begin()
  // GPIO15 enables the power circuit for display and battery
  pinMode(15, OUTPUT);
  digitalWrite(15, HIGH);
  delay(100); // Allow power circuit to stabilize

  // GPIO38 enables the backlight
  pinMode(38, OUTPUT);
  digitalWrite(38, HIGH);
  delay(100); // Allow backlight circuit to stabilize

  // Now we can safely initialize Serial and other peripherals
  Serial.begin(115200);
  delay(1000); // Wait for serial to initialize
  Serial.println("=== W5 Starting ===");
  Serial.println("Power enabled: GPIO15 (power) + GPIO38 (backlight)");

  // Initialize battery monitoring (just sets up ADC pin)
  pinMode(4, INPUT); // Battery voltage pin
  Serial.println("Battery monitoring initialized");

  Serial.println("Initializing display...");
  DisplayManager::initDisplay();
  Serial.println("Display initialized");

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