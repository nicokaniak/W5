#include <Arduino.h>
#include "config/config.h"
#include "TimeManager.h"
#include "AlarmManager.h"
#include "BluetoothManager.h"
#include "WeatherManager.h"
#include "DisplayManager.h"
#include "ButtonManager.h"
#include "MenuManager.h"

// ponytail: weather was fetched every loop tick (1/sec in the old loop).
// The new loop runs at ~50Hz for button responsiveness, which would be 50x worse.
// Throttle to 10 min — open-meteo fair-use is roughly once per ~10 min.
static const uint32_t WATCH_REDRAW_MS  = 1000;
static const uint32_t WEATHER_FETCH_MS = 600000;

static uint32_t lastWatchRedraw = 0;
static uint32_t lastWeatherFetch = 0;

void setup() {
  Serial.begin(115200);
  delay(1000); // Wait for serial to initialize
  Serial.println("=== W5 Starting ===");
  Serial.println("Initializing display...");
  DisplayManager::initDisplay();
  Serial.println("Display initialized");

  TimeManager::initTime();
  AlarmManager::initAlarms();
  BluetoothManager::initBluetooth();
  WeatherManager::initWeather();
  ButtonManager::init();
  MenuManager::init();
  Serial.println("Ready");
}

void loop() {
  // Poll buttons every iteration (~50Hz) so clicks/long-press feel responsive
  ButtonManager::update();
  ButtonEvent evt = ButtonManager::pollEvent();
  MenuManager::handleEvent(evt);

  uint32_t now = millis();

  switch (MenuManager::currentMode()) {
    case MODE_WATCH: {
      // Redraw watch face once per second, or immediately if mode just changed
      bool force = MenuManager::consumeDirty();
      if (force || now - lastWatchRedraw >= WATCH_REDRAW_MS) {
        TimeManager::updateTime();
        AlarmManager::checkAlarms();
        BluetoothManager::checkNotifications();
        DisplayManager::drawWatchFace(TimeManager::getCurrentTime());
        lastWatchRedraw = now;
      }
      if (now - lastWeatherFetch >= WEATHER_FETCH_MS) {
        WeatherManager::updateWeather();
        lastWeatherFetch = now;
      }
      break;
    }
    case MODE_MENU:
      // Redraw only on selection change (dirty flag), not every tick
      if (MenuManager::consumeDirty()) {
        DisplayManager::drawMenu(MenuManager::selectedIndex());
      }
      break;
    case MODE_STOPWATCH:
      if (MenuManager::consumeDirty()) {
        DisplayManager::clearDisplay();
        DisplayManager::drawText("Stopwatch (TODO)", 10, 30);
      }
      break;
    case MODE_CONFIG:
      if (MenuManager::consumeDirty()) {
        DisplayManager::clearDisplay();
        DisplayManager::drawText("Configuration (TODO)", 10, 30);
      }
      break;
  }

  delay(20); // ~50Hz polling
}
