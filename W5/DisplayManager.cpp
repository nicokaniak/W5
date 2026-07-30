#include "DisplayManager.h"
#include "BatteryManager.h"
#include "BluetoothManager.h"
#include "RM67162Display.h"
#include "MenuManager.h"
#include "WeatherManager.h"
#include "icons.h" // 1-bit PROGMEM menu icons (generated from icons/*.png)
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

// ponytail: crude RGB565 channel-wise lerp. Not gamma-correct, but you can't
// tell the difference on a 1.91" AMOLED during a 200ms animation.
static uint16_t lerp565(uint16_t a, uint16_t b, float t) {
  int ra = (a >> 11) & 0x1F, ga = (a >> 5) & 0x3F, ba = a & 0x1F;
  int rb = (b >> 11) & 0x1F, gb = (b >> 5) & 0x3F, bb = b & 0x1F;
  int r  = ra + (int)((rb - ra) * t);
  int g  = ga + (int)((gb - ga) * t);
  int bl = ba + (int)((bb - ba) * t);
  return (uint16_t)((r << 11) | (g << 5) | bl);
}

// Draw one menu item at a given angle on the arc. closeness (0..1) controls
// visual prominence: 1.0 = selected (large, white, bright dot), 0.0 = tiny, dim, blurred.
static void drawMenuItem(GFXcanvas16 *c, int16_t cx, int16_t cy, int16_t r,
                         float angleDeg, uint8_t labelIdx, float closeness) {
  if (closeness < 0.0f) closeness = 0.0f;
  if (closeness > 1.0f) closeness = 1.0f;

  float a = angleDeg * M_PI / 180.0f;
  int16_t ax = cx + (int16_t)(r * cosf(a));
  int16_t ay = cy + (int16_t)(r * sinf(a));

  // Dot: grows from r=2 (very dim) to r=6 (bright cyan) as it approaches center
  uint16_t dotColor = lerp565(0x2104, 0x07FF, closeness);
  uint8_t  dotRad   = 2 + (uint8_t)(closeness * 4.0f + 0.5f);
  c->fillCircle(ax, ay, dotRad, dotColor);

  String label(MenuManager::menuItemLabel(labelIdx));

  // ponytail: three visual tiers based on closeness to center.
  //   closeness >= 0.6: SELECTED — size 6, white, left-anchored at arc point
  //   closeness >= 0.25: NEAR — size 4, medium gray, left-anchored
  //   closeness < 0.25: FAR — size 2, very dim, simulated blur via 3x offset draw
  // Text is left-anchored at the arc point and extends toward the outer edge
  // (rightward) so the block reads as screen-centered, not arc-centered.
  // The blur is faked by printing the text 3 times with 1px offsets in a dim color.
  // Cheap (3 print calls instead of 1) but reads as "out of focus" on AMOLED.
  bool isSelected = (closeness >= 0.6f);
  bool isFar      = (closeness < 0.25f);

  uint8_t  textSize = isSelected ? 6 : (isFar ? 2 : 4);
  uint16_t textCol  = lerp565(0x4208, 0xFFFF, closeness);  // very dark gray -> white

  c->setTextSize(textSize);
  c->setTextColor(textCol, 0x0000);

  int16_t x1, y1;
  uint16_t w, h;
  c->getTextBounds(label, 0, 0, &x1, &y1, &w, &h);
  int16_t tx = ax - x1;       // left-anchored at arc point, extends toward outer edge
  int16_t ty = ay - h / 2 - y1;

  if (isFar) {
    // ponytail: fake blur — draw 3 copies at 1px offsets in a dimmer color.
    // No real Gaussian blur in Adafruit_GFX; this is the cheapest approximation.
    // Icons skipped on far tier — bitmaps don't fake-blur, and they'd read as
    // a hard dot next to soft text.
    uint16_t blurCol = lerp565(0x2104, 0x4208, closeness * 4.0f);  // very dim
    c->setTextColor(blurCol, 0x0000);
    c->setCursor(tx,     ty);     c->print(label);
    c->setCursor(tx + 1, ty);     c->print(label);
    c->setCursor(tx,     ty + 1); c->print(label);
  } else {
    c->setCursor(tx, ty);
    c->print(label);

    // Icon to the right of the text. Size matches the text tier:
    //   selected (size 6, 48px tall) -> 48x48 icon
    //   near      (size 4, 32px tall) -> 32x32 icon
    // Recolored with the same textCol so it dims/brightens with the carousel.
    uint8_t iconSize = isSelected ? 48 : 32;
    uint8_t iw = 0, ih = 0;
    const unsigned char *bm = menuIconBitmap(labelIdx, iconSize, &iw, &ih);
    if (bm) {
      int16_t iconX = tx + (int16_t)w + 4;  // 4px gap after text
      int16_t iconY = ay - (int16_t)ih / 2; // vertically centered on arc point
      c->drawBitmap(iconX, iconY, bm, iw, ih, textCol);
    }
  }
}

void DisplayManager::drawMenu(uint8_t selectedIndex, int8_t scrollDir, float t) {
  if (!canvas)
    return;

  canvas->fillScreen(0x0000);

  // Half-circle arc: center on LEFT edge, vertically centered, 3px thick bright cyan
  const int16_t cx = 0;
  const int16_t cy = canvas->height() / 2;   // 120
  const int16_t r  = 115;
  const uint16_t arcColor = 0x07FF;
  canvas->drawCircle(cx, cy, r - 1, arcColor);
  canvas->drawCircle(cx, cy, r,     arcColor);
  canvas->drawCircle(cx, cy, r + 1, arcColor);

  const uint8_t count = MenuManager::menuItemCount();

  if (scrollDir == 0) {
    // Static: 3 items at -60, 0, +60 degrees
    float angles[3] = { -60.0f, 0.0f, 60.0f };
    uint8_t idx[3] = {
      (uint8_t)((selectedIndex + count - 1) % count),
      selectedIndex,
      (uint8_t)((selectedIndex + 1) % count)
    };
    for (int i = 0; i < 3; i++) {
      float close = (i == 1) ? 1.0f : 0.0f;
      drawMenuItem(canvas, cx, cy, r, angles[i], idx[i], close);
    }
  } else {
    // Animated: 4 items slide along the arc.
    // scrollDir=+1 (down): items slide DOWN (angles increase). New selected
    //   enters from ABOVE (-60 -> 0), old selected slides to +60, etc.
    // scrollDir=-1 (up): items slide UP (angles decrease). New selected
    //   enters from BELOW (+60 -> 0), old selected slides to -60, etc.
    float shift = scrollDir * 60.0f * t;

    uint8_t itemIdx[4];
    float baseAngles[4];

    if (scrollDir > 0) {
      // Scrolling down. At t=0: new item enters from above.
      // [newNext, newSel, oldSel, oldPrev] at [-120, -60, 0, +60]
      itemIdx[0] = (selectedIndex + 1) % count;            // new next
      itemIdx[1] = selectedIndex;                          // new selected
      itemIdx[2] = (selectedIndex + count - 1) % count;    // old selected (now prev)
      itemIdx[3] = (selectedIndex + count - 2) % count;    // old prev (slides off bottom)
      baseAngles[0] = -120; baseAngles[1] = -60; baseAngles[2] = 0; baseAngles[3] = 60;
    } else {
      // Scrolling up. At t=0: new item enters from below.
      // [newPrev, newSel, oldSel, oldNext] at [+120, +60, 0, -60]
      itemIdx[0] = (selectedIndex + count - 1) % count;    // new prev
      itemIdx[1] = selectedIndex;                          // new selected
      itemIdx[2] = (selectedIndex + 1) % count;            // old selected (now next)
      itemIdx[3] = (selectedIndex + 2) % count;            // old next (slides off top)
      baseAngles[0] = 120; baseAngles[1] = 60; baseAngles[2] = 0; baseAngles[3] = -60;
    }

    for (int i = 0; i < 4; i++) {
      float angle = baseAngles[i] + shift;
      // Skip items that have slid off the visible half-arc
      if (angle < -95.0f || angle > 95.0f) continue;
      // Closeness to center (angle 0): 1.0 at center, 0.0 at +/-60, negative beyond
      float closeness = 1.0f - fabs(angle) / 60.0f;
      drawMenuItem(canvas, cx, cy, r, angle, itemIdx[i], closeness);
    }
  }

  pushToDisplay();
}
