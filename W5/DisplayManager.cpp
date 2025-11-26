#include "DisplayManager.h"
#include "BatteryManager.h"
#include "RM67162Display.h"
#include "WeatherManager.h"
#include "rm67162.h" // For lcd_PushColors


static RM67162Display display;
GFXcanvas16 *DisplayManager::canvas = nullptr;

void DisplayManager::initDisplay() {
  display.begin();
  display.setRotation(1);     // Landscape
  display.fillScreen(0x0000); // Clear initial screen

  // Allocate framebuffer in PSRAM if possible, otherwise heap
  // 536 * 240 * 2 = 257,280 bytes
  canvas = new GFXcanvas16(display.width(), display.height());
  if (canvas) {
    Serial.println("Canvas allocated successfully");
  } else {
    Serial.println("Canvas allocation FAILED!");
  }
}

void DisplayManager::pushToDisplay() {
  if (canvas) {
    lcd_PushColors(0, 0, canvas->width(), canvas->height(),
                   canvas->getBuffer());
  }
}

void DisplayManager::clearDisplay() {
  if (canvas)
    canvas->fillScreen(0x0000);
  pushToDisplay();
}

void DisplayManager::drawText(const String &text, int x, int y) {
  if (!canvas)
    return;
  canvas->fillScreen(0x0000);
  canvas->setTextColor(0xFFFF, 0x0000); // white on black
  canvas->setTextSize(3);
  canvas->setCursor(x, y);
  canvas->print(text);
  pushToDisplay();
}

void DisplayManager::drawWatchFace(const String &timeStr) {
  if (!canvas)
    return;

  // Draw to off-screen buffer
  canvas->fillScreen(0x0000);
  canvas->setTextColor(0x07FF, 0x0000); // cyan
  canvas->setTextSize(4);

  int16_t x1, y1;
  uint16_t w, h;
  canvas->getTextBounds(timeStr, 0, 0, &x1, &y1, &w, &h);
  int16_t x = (canvas->width() - w) / 2 - x1;
  int16_t y = (canvas->height() - h) / 2 - y1;
  canvas->setCursor(x, y);
  canvas->print(timeStr);

  // ----- Battery bar (bottom‑right) -----
  int batPct = BatteryManager::getPercentage();
  int barWidth = map(batPct, 0, 100, 0, 50); // max 50 px width
  int barX = canvas->width() - 55;           // 5 px margin from right edge
  int barY = canvas->height() - 10;          // 10 px from bottom
  // background (light gray)
  canvas->fillRect(barX, barY, 50, 8, 0x7BEF);
  // fill proportional to charge (green)
  canvas->fillRect(barX, barY, barWidth, 8, 0x07E0);

  // Push buffer to display
  pushToDisplay();
}

void DisplayManager::drawWeatherScreen() {
  if (!canvas)
    return;

  canvas->fillScreen(0x0000);

  // Title
  canvas->setTextColor(0xFFE0, 0x0000); // yellow
  canvas->setTextSize(3);
  canvas->setCursor(10, 10);
  canvas->print("WEATHER");

  // Get weather data
  String temp = WeatherManager::getTemperature();
  String conditions = WeatherManager::getWeatherDescription();
  String wind = WeatherManager::getWindSpeed();

  // Temperature
  canvas->setTextColor(0xFFFF, 0x0000); // white
  canvas->setTextSize(2);
  canvas->setCursor(10, 50);
  canvas->print("Temp: ");
  canvas->print(temp);
  canvas->print(" C");

  // Conditions
  canvas->setCursor(10, 80);
  canvas->print("Conditions:");
  canvas->setCursor(10, 105);
  canvas->setTextSize(2);
  canvas->print(conditions);

  // Wind speed
  canvas->setCursor(10, 135);
  canvas->print("Wind: ");
  canvas->print(wind);
  canvas->print(" km/h");

  // Location note
  canvas->setTextColor(0x7BEF, 0x0000); // gray
  canvas->setTextSize(1);
  canvas->setCursor(10, 170);
  canvas->print("Location: Copenhagen");

  // Update hint
  canvas->setCursor(10, 185);
  canvas->print("Updates every 60s");

  pushToDisplay();
}

void DisplayManager::drawAlarmsScreen() {
  if (!canvas)
    return;

  canvas->fillScreen(0x0000);

  // Title
  canvas->setTextColor(0xF81F, 0x0000); // magenta
  canvas->setTextSize(3);
  canvas->setCursor(10, 10);
  canvas->print("ALARMS");

  // Alarm info
  canvas->setTextColor(0xFFFF, 0x0000); // white
  canvas->setTextSize(2);
  canvas->setCursor(10, 50);
  canvas->print("Alarm 1: --:--");

  canvas->setCursor(10, 80);
  canvas->print("Status: ");
  canvas->print("Inactive");

  // Note
  canvas->setTextColor(0x7BEF, 0x0000); // gray
  canvas->setTextSize(1);
  canvas->setCursor(10, 120);
  canvas->print("Use app to set alarms");

  pushToDisplay();
}

void DisplayManager::drawBatteryScreen() {
  if (!canvas)
    return;

  canvas->fillScreen(0x0000);

  // Title
  canvas->setTextColor(0x07E0, 0x0000); // green
  canvas->setTextSize(3);
  canvas->setCursor(10, 10);
  canvas->print("BATTERY");

  // Get battery data
  float batVolt = BatteryManager::getVoltage();
  int batPct = BatteryManager::getPercentage();

  // Voltage
  canvas->setTextColor(0xFFFF, 0x0000); // white
  canvas->setTextSize(2);
  canvas->setCursor(10, 50);
  canvas->print("Voltage: ");
  canvas->print(batVolt, 2);
  canvas->print("V");

  // Percentage
  canvas->setCursor(10, 80);
  canvas->print("Charge: ");
  canvas->print(batPct);
  canvas->print("%");

  // Large battery bar visualization
  int barWidth = map(batPct, 0, 100, 0, 200); // max 200 px width
  int barX = 10;
  int barY = 120;

  // Border
  canvas->drawRect(barX - 2, barY - 2, 204, 34, 0xFFFF);

  // Background (dark gray)
  canvas->fillRect(barX, barY, 200, 30, 0x2104);

  // Fill based on percentage
  uint16_t barColor;
  if (batPct > 50) {
    barColor = 0x07E0; // green
  } else if (batPct > 20) {
    barColor = 0xFFE0; // yellow
  } else {
    barColor = 0xF800; // red
  }
  canvas->fillRect(barX, barY, barWidth, 30, barColor);

  // Status text
  canvas->setTextColor(0x7BEF, 0x0000); // gray
  canvas->setTextSize(1);
  canvas->setCursor(10, 170);
  canvas->print("GPIO15: Power enabled");

  pushToDisplay();
}

void DisplayManager::drawBluetoothScreen() {
  if (!canvas)
    return;

  canvas->fillScreen(0x0000);

  // Title
  canvas->setTextColor(0x001F, 0x0000); // blue
  canvas->setTextSize(3);
  canvas->setCursor(10, 10);
  canvas->print("BLUETOOTH");

  // Connection status
  canvas->setTextColor(0xFFFF, 0x0000); // white
  canvas->setTextSize(2);
  canvas->setCursor(10, 50);
  canvas->print("Status: ");
  canvas->print("Ready");

  // Notifications
  canvas->setCursor(10, 80);
  canvas->print("Notifications:");

  canvas->setTextSize(1);
  canvas->setCursor(10, 110);
  canvas->print("No new notifications");

  // Note
  canvas->setTextColor(0x7BEF, 0x0000); // gray
  canvas->setCursor(10, 140);
  canvas->print("Pair with phone for alerts");

  pushToDisplay();
}
