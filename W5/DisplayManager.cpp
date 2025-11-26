#include "DisplayManager.h"
#include "BatteryManager.h"
#include "RM67162Display.h"
#include "WeatherManager.h"

static RM67162Display display;

void DisplayManager::initDisplay() {
  display.begin();
  display.setRotation(1);
  display.fillScreen(0x0000); // black
}

void DisplayManager::clearDisplay() { display.fillScreen(0x0000); }

void DisplayManager::drawText(const String &text, int x, int y) {
  display.setTextColor(0xFFFF, 0x0000); // white on black
  display.setTextSize(3);
  display.setCursor(x, y);
  display.print(text);
}

void DisplayManager::drawWatchFace(const String &timeStr) {
  display.fillScreen(0x0000);
  display.setTextColor(0x07FF, 0x0000); // cyan
  display.setTextSize(4);

  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(timeStr, 0, 0, &x1, &y1, &w, &h);
  int16_t x = (display.width() - w) / 2 - x1;
  int16_t y = (display.height() - h) / 2 - y1;
  display.setCursor(x, y);
  display.print(timeStr);

  // ----- Battery bar (bottom‑right) -----
  int batPct = BatteryManager::getPercentage();
  int barWidth = map(batPct, 0, 100, 0, 50); // max 50 px width
  int barX = display.width() - 55;           // 5 px margin from right edge
  int barY = display.height() - 10;          // 10 px from bottom
  // background (light gray)
  display.fillRect(barX, barY, 50, 8, 0x7BEF);
  // fill proportional to charge (green)
  display.fillRect(barX, barY, barWidth, 8, 0x07E0);
}

void DisplayManager::drawWeatherScreen() {
  display.fillScreen(0x0000);

  // Title
  display.setTextColor(0xFFE0, 0x0000); // yellow
  display.setTextSize(3);
  display.setCursor(10, 10);
  display.print("WEATHER");

  // Get weather data
  String temp = WeatherManager::getTemperature();
  String conditions = WeatherManager::getWeatherDescription();
  String wind = WeatherManager::getWindSpeed();

  // Temperature
  display.setTextColor(0xFFFF, 0x0000); // white
  display.setTextSize(2);
  display.setCursor(10, 50);
  display.print("Temp: ");
  display.print(temp);
  display.print(" C");

  // Conditions
  display.setCursor(10, 80);
  display.print("Conditions:");
  display.setCursor(10, 105);
  display.setTextSize(2);
  display.print(conditions);

  // Wind speed
  display.setCursor(10, 135);
  display.print("Wind: ");
  display.print(wind);
  display.print(" km/h");

  // Location note
  display.setTextColor(0x7BEF, 0x0000); // gray
  display.setTextSize(1);
  display.setCursor(10, 170);
  display.print("Location: Copenhagen");

  // Update hint
  display.setCursor(10, 185);
  display.print("Updates every 60s");
}

void DisplayManager::drawAlarmsScreen() {
  display.fillScreen(0x0000);

  // Title
  display.setTextColor(0xF81F, 0x0000); // magenta
  display.setTextSize(3);
  display.setCursor(10, 10);
  display.print("ALARMS");

  // Alarm info
  display.setTextColor(0xFFFF, 0x0000); // white
  display.setTextSize(2);
  display.setCursor(10, 50);
  display.print("Alarm 1: --:--");

  display.setCursor(10, 80);
  display.print("Status: ");
  display.print("Inactive");

  // Note
  display.setTextColor(0x7BEF, 0x0000); // gray
  display.setTextSize(1);
  display.setCursor(10, 120);
  display.print("Use app to set alarms");
}

void DisplayManager::drawBatteryScreen() {
  display.fillScreen(0x0000);

  // Title
  display.setTextColor(0x07E0, 0x0000); // green
  display.setTextSize(3);
  display.setCursor(10, 10);
  display.print("BATTERY");

  // Get battery data
  float batVolt = BatteryManager::getVoltage();
  int batPct = BatteryManager::getPercentage();

  // Voltage
  display.setTextColor(0xFFFF, 0x0000); // white
  display.setTextSize(2);
  display.setCursor(10, 50);
  display.print("Voltage: ");
  display.print(batVolt, 2);
  display.print("V");

  // Percentage
  display.setCursor(10, 80);
  display.print("Charge: ");
  display.print(batPct);
  display.print("%");

  // Large battery bar visualization
  int barWidth = map(batPct, 0, 100, 0, 200); // max 200 px width
  int barX = 10;
  int barY = 120;

  // Border
  display.drawRect(barX - 2, barY - 2, 204, 34, 0xFFFF);

  // Background (dark gray)
  display.fillRect(barX, barY, 200, 30, 0x2104);

  // Fill based on percentage
  uint16_t barColor;
  if (batPct > 50) {
    barColor = 0x07E0; // green
  } else if (batPct > 20) {
    barColor = 0xFFE0; // yellow
  } else {
    barColor = 0xF800; // red
  }
  display.fillRect(barX, barY, barWidth, 30, barColor);

  // Status text
  display.setTextColor(0x7BEF, 0x0000); // gray
  display.setTextSize(1);
  display.setCursor(10, 170);
  display.print("GPIO15: Power enabled");
}

void DisplayManager::drawBluetoothScreen() {
  display.fillScreen(0x0000);

  // Title
  display.setTextColor(0x001F, 0x0000); // blue
  display.setTextSize(3);
  display.setCursor(10, 10);
  display.print("BLUETOOTH");

  // Connection status
  display.setTextColor(0xFFFF, 0x0000); // white
  display.setTextSize(2);
  display.setCursor(10, 50);
  display.print("Status: ");
  display.print("Ready");

  // Notifications
  display.setCursor(10, 80);
  display.print("Notifications:");

  display.setTextSize(1);
  display.setCursor(10, 110);
  display.print("No new notifications");

  // Note
  display.setTextColor(0x7BEF, 0x0000); // gray
  display.setCursor(10, 140);
  display.print("Pair with phone for alerts");
}
