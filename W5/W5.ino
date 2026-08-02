#include <Arduino.h>
#include <driver/gpio.h> // For GPIO hold functionality
#include "config/config.h"
#include "AlarmManager.h"
#include "BatteryManager.h"
#include "BluetoothManager.h"
#include "ButtonManager.h"
#include "DisplayManager.h"
#include "MenuManager.h"
#include "StopwatchManager.h"
#include "TimeManager.h"
#include "WeatherManager.h"

// ponytail: weather was fetched every loop tick (1/sec in the old loop).
// The new loop runs at ~50Hz for button responsiveness, which would be 50x worse.
// Throttle to 10 min — open-meteo fair-use is roughly once per ~10 min.
static const uint32_t WATCH_REDRAW_MS  = 1000;
static const uint32_t WEATHER_FETCH_MS = 600000;
// ponytail: centiseconds need >1Hz to feel alive, but full framebuffer push
// (~257KB @ 75MHz SPI ≈ 27ms) rules out 100Hz. 20Hz (50ms) reads as a live
// stopwatch without saturating SPI. Ceiling: cs digit jumps ~5/frame; value is
// always exact since elapsed is computed from millis() at draw time.
static const uint32_t STOPWATCH_REDRAW_MS = 50;

static uint32_t lastWatchRedraw = 0;
static uint32_t lastWeatherFetch = 0;
static uint32_t lastStopwatchRedraw = 0;

void setup() {
  // ===== CRITICAL: ENABLE POWER FIRST - BEFORE ANYTHING ELSE =====
  // GPIO15 enables the power circuit for display and battery
  pinMode(15, OUTPUT);
  digitalWrite(15, HIGH);
  gpio_hold_en((gpio_num_t)15);
  delay(100);

  // GPIO38 enables the backlight
  pinMode(38, OUTPUT);
  digitalWrite(38, HIGH);
  gpio_hold_en((gpio_num_t)38);
  delay(100);

  Serial.begin(115200);
  delay(1000);
  Serial.println("=== W5 Starting ===");
  Serial.println("Power enabled: GPIO15 + GPIO38 with GPIO hold");

  // Battery monitoring ADC pin
  pinMode(4, INPUT);
  Serial.println("Battery monitoring initialized");

  Serial.println("Initializing display...");
  DisplayManager::initDisplay();
  Serial.println("Display initialized");

  // WiFi first so NTP can sync
  WeatherManager::initWeather();

  TimeManager::initTime();
  AlarmManager::initAlarms();
  BluetoothManager::initBluetooth();
  ButtonManager::init();
  MenuManager::init();
  StopwatchManager::init();
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
      if (MenuManager::isAnimating()) {
        // Drive the slide animation: redraw every frame with interpolated positions
        MenuManager::updateAnimation();
        DisplayManager::drawMenu(MenuManager::selectedIndex(),
                                 MenuManager::scrollDir(),
                                 MenuManager::animProgress());
      } else if (MenuManager::consumeDirty()) {
        // Static redraw (mode entry, animation just finished, etc.)
        DisplayManager::drawMenu(MenuManager::selectedIndex());
      }
      break;
    case MODE_STOPWATCH: {
      // Redraw on mode entry / state change, or at 20Hz while running so the
      // centiseconds tick. Idle/stopped screens are static (no periodic redraw).
      bool force = MenuManager::consumeDirty() || StopwatchManager::consumeDirty();
      if (force || (StopwatchManager::isRunning() &&
                    now - lastStopwatchRedraw >= STOPWATCH_REDRAW_MS)) {
        DisplayManager::drawStopwatch();
        lastStopwatchRedraw = now;
      }
      break;
    }
    case MODE_WEATHER: {
      bool force = MenuManager::consumeDirty();
      // Refresh weather data every 10 min even while viewing the screen
      if (now - lastWeatherFetch >= WEATHER_FETCH_MS) {
        WeatherManager::updateWeather();
        lastWeatherFetch = now;
        force = true;
      }
      if (force) {
        DisplayManager::drawWeatherScreen();
      }
      break;
    }
    case MODE_CONFIG:
      if (MenuManager::consumeDirty()) {
        DisplayManager::drawConfigMenu(MenuManager::configSelectedIndex());
      }
      break;
  }

  delay(10); // ~100Hz — doubles animation frame rate vs 50Hz for smoother scrolling
}
