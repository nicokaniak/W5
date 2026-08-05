#include <Arduino.h>
#include <driver/gpio.h> // For GPIO hold functionality
#include "config/config.h"
#include "BatteryManager.h"
#include "BluetoothManager.h"
#include "ButtonManager.h"
#include "DisplayManager.h"
#include "MenuManager.h"
#include "StopwatchManager.h"
#include "PomodoroManager.h"
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
// ponytail: Pomodoro countdown only needs 1Hz — seconds tick, no centiseconds.
// Ceiling: the display reads as a live countdown; sub-second updates would just
// burn SPI bandwidth for no perceptible change.
static const uint32_t POMODORO_REDRAW_MS = 1000;
// ponytail: ambient menu animation (radar sweep, dot pulse, scanlines, flicker)
// runs at 20fps even when idle. Full framebuffer push (~27ms @ 75MHz SPI) caps
// us near 30fps; 50ms is safe and reads as smooth. Ceiling: if styles grow
// heavier per-frame draw work, drop to 15fps (66ms) before raising SPI clock.
static const uint32_t MENU_REDRAW_MS = 50;

static uint32_t lastWatchRedraw = 0;
static uint32_t lastWeatherFetch = 0;
static uint32_t lastStopwatchRedraw = 0;
static uint32_t lastPomodoroRedraw = 0;
static uint32_t lastMenuRedraw = 0;

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
  // Start ESP32-S3 USB-Serial-JTAG CDC so bool(HWCDCSerial) reflects host
  // connection for the charging-icon check. No-op if CDC-on-boot is on.
  HWCDCSerial.begin();
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

  // ponytail: fetch weather immediately at boot (sync) so the first weather-screen
  // open has data; otherwise the user waits up to 10 min for the periodic refresh.
  // Subsequent calls from loop() use the default async=true so the 16s HTTP
  // window runs on the fetch task (core 0) and doesn't stall button polling.
  WeatherManager::updateWeather(false);
  lastWeatherFetch = millis();

  BluetoothManager::initBluetooth();
  ButtonManager::init();
  MenuManager::init();
  StopwatchManager::init();
  PomodoroManager::init();
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
        lastMenuRedraw = now;
      } else if (MenuManager::consumeDirty()) {
        // Static redraw (mode entry, animation just finished, etc.)
        DisplayManager::drawMenu(MenuManager::selectedIndex());
        lastMenuRedraw = now;
      } else if (now - lastMenuRedraw >= MENU_REDRAW_MS) {
        // Ambient: styles use millis()-driven effects (sweep/pulse/scanlines)
        // that need continuous redraw even when the carousel is idle.
        DisplayManager::drawMenu(MenuManager::selectedIndex());
        lastMenuRedraw = now;
      }
      break;
    case MODE_TRANSITION: {
      // Dive/zoom: redraw every frame until the animation finishes, then
      // updateAnimation() resolves _mode into the target screen.
      MenuManager::updateAnimation();
      DisplayManager::drawTransition(MenuManager::selectedIndex(),
                                     MenuManager::animProgress());
      lastMenuRedraw = now;
      break;
    }
    case MODE_MENU_STYLE: {
      // Style picker: redraw on change or at 20fps for the reticle/ambient feel.
      bool force = MenuManager::consumeDirty();
      if (force || now - lastMenuRedraw >= MENU_REDRAW_MS) {
        DisplayManager::drawMenuStylePicker((uint8_t)MenuManager::menuStylePickerIndex());
        lastMenuRedraw = now;
      }
      break;
    }
    case MODE_BRIGHTNESS: {
      // Brightness picker: redraw on change. No ambient effects, so only redraw
      // when dirty (the live preview is driven by lcd_brightness(), not the UI).
      bool force = MenuManager::consumeDirty();
      if (force || now - lastMenuRedraw >= MENU_REDRAW_MS) {
        DisplayManager::drawBrightnessPicker(MenuManager::brightnessPickerIndex());
        lastMenuRedraw = now;
      }
      break;
    }
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
    case MODE_POMODORO: {
      // Auto-advance when the current phase timer expires.
      PomodoroManager::update();
      // Redraw on mode entry / state change, or at 1Hz while running so the
      // countdown ticks. Idle/paused screens are static.
      bool force = MenuManager::consumeDirty() || PomodoroManager::consumeDirty();
      if (force || (PomodoroManager::isRunning() &&
                    now - lastPomodoroRedraw >= POMODORO_REDRAW_MS)) {
        DisplayManager::drawPomodoro();
        lastPomodoroRedraw = now;
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

  // ponytail: 10ms keeps button polling at ~100Hz (debounce window is 20ms, so
  // 50Hz would also catch every transition, but the extra headroom costs nothing
  // — the ESP32 isn't sleep-bound here). Redraw paths are independently throttled
  // to 20-50Hz via the *_REDRAW_MS constants, so this only affects input latency.
  delay(10);
}
