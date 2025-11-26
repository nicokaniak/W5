#include "AlarmManager.h"
#include "BatteryManager.h"
#include "BluetoothManager.h"
#include "DisplayManager.h"
#include "MenuManager.h"
#include "TimeManager.h"
#include "WeatherManager.h"
#include "config/config.h"
#include <Arduino.h>
#include <driver/gpio.h> // For GPIO hold functionality

void setup() {
  // ===== CRITICAL: ENABLE POWER FIRST - BEFORE ANYTHING ELSE =====
  // This MUST be the very first thing, even before Serial.begin()
  // GPIO15 enables the power circuit for display and battery
  pinMode(15, OUTPUT);
  digitalWrite(15, HIGH);

  // IMPORTANT: Enable GPIO hold to maintain HIGH state during power transitions
  // This helps prevent the board from shutting down when USB is disconnected
  gpio_hold_en((gpio_num_t)15);

  delay(100); // Allow power circuit to stabilize

  // GPIO38 enables the backlight
  pinMode(38, OUTPUT);
  digitalWrite(38, HIGH);

  // Also enable hold for backlight to keep display on
  gpio_hold_en((gpio_num_t)38);

  delay(100); // Allow backlight circuit to stabilize

  // Now we can safely initialize Serial and other peripherals
  Serial.begin(115200);
  delay(1000); // Wait for serial to initialize
  Serial.println("=== W5 Starting ===");
  Serial.println(
      "Power enabled: GPIO15 (power) + GPIO38 (backlight) with GPIO hold");

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

  // Initialize menu system
  MenuManager::initMenu();
}

void loop() {
  // Handle button input for menu navigation
  MenuManager::handleButtons();

  // Continue background tasks
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

  // Display appropriate screen based on current menu state
  MenuScreen currentScreen = MenuManager::getCurrentScreen();

  switch (currentScreen) {
  case WATCH_FACE:
    DisplayManager::drawWatchFace(TimeManager::getCurrentTime());
    break;
  case WEATHER_SCREEN:
    DisplayManager::drawWeatherScreen();
    break;
  case ALARMS_SCREEN:
    DisplayManager::drawAlarmsScreen();
    break;
  case BATTERY_SCREEN:
    DisplayManager::drawBatteryScreen();
    break;
  case BLUETOOTH_SCREEN:
    DisplayManager::drawBluetoothScreen();
    break;
  }

  delay(100); // Update 10 times per second for responsive button handling
}