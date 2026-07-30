#include "DisplayManager.h"
#include "BatteryManager.h"
#include "BluetoothManager.h"
#include "RM67162Display.h"
#include "MenuManager.h"
#include "WeatherManager.h"
#include "rm67162.h" // For lcd_PushColors
#include <math.h>

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

  // ----- Battery bar (bottom-right) -----
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
  bool connected = BluetoothManager::isConnected();

  canvas->setTextColor(0xFFFF, 0x0000); // white
  canvas->setTextSize(2);
  canvas->setCursor(10, 50);
  canvas->print("Status: ");

  if (connected) {
    canvas->setTextColor(0x07E0, 0x0000); // green
    canvas->print("Connected");
  } else {
    canvas->setTextColor(0xF800, 0x0000); // red
    canvas->print("Disconnected");
  }

  // Device Name
  canvas->setTextColor(0xFFFF, 0x0000); // white
  canvas->setCursor(10, 80);
  canvas->print("Device: Lilygo_Watch");

  // Notifications
  canvas->setCursor(10, 110);
  canvas->print("Last Message:");

  canvas->setTextSize(1);
  canvas->setCursor(10, 140);
  String note = BluetoothManager::getNotification();
  if (note.length() > 0) {
    canvas->print(note);
  } else {
    canvas->print("No new messages");
  }

  // Instructions
  if (!connected) {
    canvas->setTextColor(0x7BEF, 0x0000); // gray
    canvas->setCursor(10, 170);
    canvas->print("Pair with 'Lilygo_Watch'");
    canvas->setCursor(10, 185);
    canvas->print("Use Serial Bluetooth Terminal");
  }

  pushToDisplay();
}

void DisplayManager::drawMenu(uint8_t selectedIndex) {
  if (!canvas)
    return;

  canvas->fillScreen(0x0000);  // black

  // Half-circle: center on the LEFT edge (x=0), vertically centered.
  // The left half is off-screen, so drawCircle's bounds check crops it for free.
  // Draw 3 concentric circles for a thick, visible arc (1px was invisible on AMOLED).
  const int16_t cx = 0;
  const int16_t cy = canvas->height() / 2;   // 120
  const int16_t r  = 115;
  const uint16_t arcColor = 0x07FF;          // bright cyan — was 0x8410 (invisible dim gray)
  canvas->drawCircle(cx, cy, r - 1, arcColor);
  canvas->drawCircle(cx, cy, r,     arcColor);
  canvas->drawCircle(cx, cy, r + 1, arcColor);

  // 3 items on the visible right half of the arc:
  //   selected at angle 0   (3 o'clock, rightmost = (r, cy))
  //   previous at -60 deg   (upper)
  //   next     at +60 deg   (lower)
  const float anglesDeg[3] = { -60.0f, 0.0f, 60.0f };
  const uint8_t count = MenuManager::menuItemCount();
  uint8_t prevIdx = (selectedIndex + count - 1) % count;
  uint8_t nextIdx = (selectedIndex + 1) % count;
  uint8_t indices[3] = { prevIdx, selectedIndex, nextIdx };

  for (int i = 0; i < 3; i++) {
    float a = anglesDeg[i] * M_PI / 180.0f;
    int16_t ax = cx + (int16_t)(r * cosf(a));
    int16_t ay = cy + (int16_t)(r * sinf(a));
    bool selected = (i == 1);

    // Filled dot at each item position on the arc — makes the carousel structure visible
    // Selected: bigger cyan dot. Others: smaller dark-cyan dot.
    if (selected) {
      canvas->fillCircle(ax, ay, 6, 0x07FF);  // bright cyan, radius 6
    } else {
      canvas->fillCircle(ax, ay, 3, 0x4210);  // dim cyan, radius 3
    }

    String label(MenuManager::menuItemLabel(indices[i]));

    // Selected: size 3 white, centered on arc point.
    // Others:   size 2 gray, left-anchored at arc point (text grows rightward into screen).
    canvas->setTextSize(selected ? 3 : 2);
    canvas->setTextColor(selected ? 0xFFFF : 0x8410, 0x0000);  // white vs gray

    int16_t x1, y1;
    uint16_t w, h;
    canvas->getTextBounds(label, 0, 0, &x1, &y1, &w, &h);
    int16_t tx, ty;
    if (selected) {
      tx = ax - w / 2 - x1;
      ty = ay - h / 2 - y1;
    } else {
      tx = ax - x1;            // left edge at arc point
      ty = ay - h / 2 - y1;    // vertically centered on arc point
    }
    canvas->setCursor(tx, ty);
    canvas->print(label);
  }

  pushToDisplay();
}
