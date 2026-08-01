#include "DisplayManager.h"
#include "BatteryManager.h"
#include "BluetoothManager.h"
#include "RM67162Display.h"
#include "MenuManager.h"
#include "StopwatchManager.h"
#include "TimeManager.h"
#include "WeatherManager.h"
#include "icons.h" // 1-bit PROGMEM menu icons (generated from icons/*.png)
#include "rm67162.h" // For lcd_PushColors
#include <math.h>

static RM67162Display display;
GFXcanvas16 *DisplayManager::canvas = nullptr;

// ----- Dot-matrix text rendering (classic LED-matrix look) -----
// Prints text to a 1-bit scratch buffer with the built-in glcdfont at size 1,
// then stamps each set pixel as a filled circle pitched by `size`. Reuses the
// font already shipped with Adafruit_GFX — no external font file or dependency.
// Bounds are cell-based and symmetric (dots centered in their cells), so
// centering the cell block also centers the ink: w = strlen*6*size, h = 8*size.
// ponytail: fixed 536x8 scratch (one screen-width at size 1). Ceiling: strings
// longer than ~89 chars clip. Upgrade: dynamic scratch or line-wrap.
static const int16_t DOT_SCRATCH_W = 536;
static const int16_t DOT_SCRATCH_H = 8;
static GFXcanvas1 *g_dotScratch = nullptr;

static uint8_t dotRadius(uint8_t size) {
  if (size >= 4) return size / 2 - 1; // 8->3, 6->2, 4->1 (1px gap between dots)
  if (size >= 2) return 1;            // 3->1, 2->1 (dots touch, chunky)
  return 0;                           // 1 -> single pixel (no dot aesthetic)
}

static void dotTextBounds(const char *s, uint8_t size, uint16_t *w, uint16_t *h) {
  if (!s) {
    if (w) *w = 0;
    if (h) *h = 0;
    return;
  }
  if (w) *w = (uint16_t)strlen(s) * 6 * size;
  if (h) *h = (uint16_t)8 * size;
}

static void dotText(GFXcanvas16 *c, const char *s, int16_t x, int16_t y,
                    uint8_t size, uint16_t color) {
  if (!c || !g_dotScratch || !s || !*s)
    return;
  const uint8_t pitch = size;
  const uint8_t r = dotRadius(size);

  // Render the string to the 1-bit scratch at size 1.
  g_dotScratch->fillScreen(0);
  g_dotScratch->setCursor(0, 0);
  g_dotScratch->setTextColor(1, 0);
  g_dotScratch->setTextSize(1);
  g_dotScratch->print(s);

  // Stamp each set pixel as a dot, centered in its pitch-sized cell.
  int16_t textW = (int16_t)strlen(s) * 6;
  if (textW > DOT_SCRATCH_W)
    textW = DOT_SCRATCH_W;
  const int16_t half = pitch / 2;
  for (int16_t row = 0; row < DOT_SCRATCH_H; row++) {
    for (int16_t col = 0; col < textW; col++) {
      if (!g_dotScratch->getPixel(col, row))
        continue;
      int16_t dx = x + col * pitch + half;
      int16_t dy = y + row * pitch + half;
      if (r == 0)
        c->drawPixel(dx, dy, color);
      else
        c->fillCircle(dx, dy, r, color);
    }
  }
}

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

  // 1-bit scratch for dot-matrix text rendering (536x8 = 536 bytes).
  g_dotScratch = new GFXcanvas1(DOT_SCRATCH_W, DOT_SCRATCH_H);
  if (g_dotScratch) {
    Serial.println("Dot-matrix scratch allocated");
  } else {
    Serial.println("Dot-matrix scratch allocation FAILED!");
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
  dotText(canvas, text.c_str(), x, y, 3, 0xFFFF); // white, size 3
  pushToDisplay();
}

void DisplayManager::drawWatchFace(const String &timeStr) {
  if (!canvas)
    return;

  // Draw to off-screen buffer
  canvas->fillScreen(0x0000);

  uint16_t w, h;
  dotTextBounds(timeStr.c_str(), 8, &w, &h);
  int16_t x = (canvas->width() - w) / 2;
  int16_t y = (canvas->height() - h) / 2;
  dotText(canvas, timeStr.c_str(), x, y, 8, 0x07FF); // cyan, size 8

  // ----- Date (top-left, small) -----
  String dateStr = TimeManager::getCurrentDate();
  if (dateStr.length()) {
    dotText(canvas, dateStr.c_str(), 8, 8, 4, 0x07FF); // cyan, same as time
  }

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
  dotText(canvas, "WEATHER", 10, 10, 3, 0xFFE0); // yellow, size 3

  // Get weather data
  String temp = WeatherManager::getTemperature();
  String conditions = WeatherManager::getWeatherDescription();
  String wind = WeatherManager::getWindSpeed();

  // Temperature
  dotText(canvas, ("Temp: " + temp + " C").c_str(), 10, 50, 2, 0xFFFF); // white

  // Conditions
  dotText(canvas, "Conditions:", 10, 80, 2, 0xFFFF);
  dotText(canvas, conditions.c_str(), 10, 105, 2, 0xFFFF);

  // Wind speed
  dotText(canvas, ("Wind: " + wind + " km/h").c_str(), 10, 135, 2, 0xFFFF);

  // Location note
  dotText(canvas, "Location: Copenhagen", 10, 170, 1, 0x7BEF); // gray

  // Update hint
  dotText(canvas, "Updates every 60s", 10, 185, 1, 0x7BEF);

  pushToDisplay();
}

void DisplayManager::drawAlarmsScreen() {
  if (!canvas)
    return;

  canvas->fillScreen(0x0000);

  // Title
  dotText(canvas, "ALARMS", 10, 10, 3, 0xF81F); // magenta, size 3

  // Alarm info
  dotText(canvas, "Alarm 1: --:--", 10, 50, 2, 0xFFFF); // white
  dotText(canvas, "Status: Inactive", 10, 80, 2, 0xFFFF);

  // Note
  dotText(canvas, "Use app to set alarms", 10, 120, 1, 0x7BEF); // gray

  pushToDisplay();
}

void DisplayManager::drawBatteryScreen() {
  if (!canvas)
    return;

  canvas->fillScreen(0x0000);

  // Title
  dotText(canvas, "BATTERY", 10, 10, 3, 0x07E0); // green, size 3

  // Get battery data
  float batVolt = BatteryManager::getVoltage();
  int batPct = BatteryManager::getPercentage();

  // Voltage
  dotText(canvas, ("Voltage: " + String(batVolt, 2) + "V").c_str(), 10, 50, 2,
          0xFFFF); // white

  // Percentage
  dotText(canvas, ("Charge: " + String(batPct) + "%").c_str(), 10, 80, 2,
          0xFFFF);

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
  dotText(canvas, "GPIO15: Power enabled", 10, 170, 1, 0x7BEF); // gray

  pushToDisplay();
}

void DisplayManager::drawBluetoothScreen() {
  if (!canvas)
    return;

  canvas->fillScreen(0x0000);

  // Title
  dotText(canvas, "BLUETOOTH", 10, 10, 3, 0x001F); // blue, size 3

  // Connection status
  bool connected = BluetoothManager::isConnected();

  // "Status: " in white, then the state word in green/red on the same line.
  dotText(canvas, "Status: ", 10, 50, 2, 0xFFFF); // white
  uint16_t stW, stH;
  dotTextBounds("Status: ", 2, &stW, &stH);
  if (connected) {
    dotText(canvas, "Connected", 10 + (int16_t)stW, 50, 2, 0x07E0); // green
  } else {
    dotText(canvas, "Disconnected", 10 + (int16_t)stW, 50, 2, 0xF800); // red
  }

  // Device Name
  dotText(canvas, "Device: Lilygo_Watch", 10, 80, 2, 0xFFFF); // white

  // Notifications
  dotText(canvas, "Last Message:", 10, 110, 2, 0xFFFF);
  String note = BluetoothManager::getNotification();
  if (note.length() > 0) {
    dotText(canvas, note.c_str(), 10, 140, 1, 0xFFFF);
  } else {
    dotText(canvas, "No new messages", 10, 140, 1, 0xFFFF);
  }

  // Instructions
  if (!connected) {
    dotText(canvas, "Pair with 'Lilygo_Watch'", 10, 170, 1, 0x7BEF); // gray
    dotText(canvas, "Use Serial Bluetooth Terminal", 10, 185, 1, 0x7BEF);
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

void DisplayManager::drawWifiConnecting() {
  if (!canvas)
    return;

  canvas->fillScreen(0x0000);

  // Pulse: 0..1 oscillation, ~1.9s period
  float phase = sinf(millis() / 300.0f) * 0.5f + 0.5f;
  uint16_t iconColor = lerp565(0x2104, 0x07FF, phase); // dark gray -> bright cyan

  // Centered 48x48 wifi icon, slightly above center to leave room for text
  int16_t iconX = (canvas->width() - 48) / 2;   // 244
  int16_t iconY = (canvas->height() - 48) / 2 - 15; // 81
  canvas->drawBitmap(iconX, iconY, ICON_WIFI_48, 48, 48, iconColor);

  // "Connecting..." text below
  const char *msg = "Connecting...";
  uint16_t w, h;
  dotTextBounds(msg, 2, &w, &h);
  dotText(canvas, msg, (canvas->width() - w) / 2, iconY + 48 + 10, 2,
          0x7BEF); // gray

  pushToDisplay();
}

// ponytail: MM:SS.CC from milliseconds. Caps at 99 minutes; a wrist stopwatch
// session rarely exceeds that. Ceiling: rolls over at 100 min. Upgrade: hours.
static String formatSw(uint32_t ms) {
  uint32_t totalCs = ms / 10;
  uint32_t s = totalCs / 100;
  uint32_t m = s / 60;
  uint32_t cs = totalCs % 100;
  s = s % 60;
  m = m % 100;
  char buf[12];
  snprintf(buf, sizeof(buf), "%02u:%02u.%02u", (unsigned)m, (unsigned)s, (unsigned)cs);
  return String(buf);
}

void DisplayManager::drawStopwatch() {
  if (!canvas)
    return;

  canvas->fillScreen(0x0000);

  // ----- Status label (top, centered) -----
  const char *status;
  uint16_t statusColor;
  if (StopwatchManager::isRunning()) {
    status = "RUNNING";
    statusColor = 0x07E0; // green
  } else if (StopwatchManager::isStopped()) {
    status = "STOPPED";
    statusColor = 0xF800; // red
  } else {
    status = "READY";
    statusColor = 0x07FF; // cyan
  }

  uint16_t w, h;
  dotTextBounds(status, 2, &w, &h);
  dotText(canvas, status, (canvas->width() - w) / 2, 12, 2, statusColor);

  // ----- Big elapsed time (centered, textSize 8) -----
  String timeStr = formatSw(StopwatchManager::getElapsedMs());
  dotTextBounds(timeStr.c_str(), 8, &w, &h);
  dotText(canvas, timeStr.c_str(), (canvas->width() - w) / 2,
          (canvas->height() - h) / 2, 8, 0xFFFF); // white

  // ----- Lap line (below time) or controls hint -----
  uint8_t laps = StopwatchManager::getLapCount();
  if (laps > 0) {
    String lapStr = "LAP " + String(laps) + ": " + formatSw(StopwatchManager::getLastLapMs());
    dotTextBounds(lapStr.c_str(), 2, &w, &h);
    dotText(canvas, lapStr.c_str(), (canvas->width() - w) / 2, 170, 2,
            0xFFE0); // yellow
  }

  // Controls hint (bottom, tiny gray)
  const char *hint = "TOP:START/STOP  BOT:LAP/RST";
  dotTextBounds(hint, 1, &w, &h);
  dotText(canvas, hint, (canvas->width() - w) / 2, 222, 1, 0x7BEF); // gray

  pushToDisplay();
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

  uint16_t w, h;
  dotTextBounds(label.c_str(), textSize, &w, &h);
  int16_t tx = ax;            // left-anchored at arc point, extends toward outer edge
  int16_t ty = ay - (int16_t)h / 2;

  if (isFar) {
    // ponytail: fake blur — draw 3 copies at 1px offsets in a dimmer color.
    // No real Gaussian blur in Adafruit_GFX; this is the cheapest approximation.
    // Icons skipped on far tier — bitmaps don't fake-blur, and they'd read as
    // a hard dot next to soft text.
    uint16_t blurCol = lerp565(0x2104, 0x4208, closeness * 4.0f);  // very dim
    dotText(c, label.c_str(), tx,     ty,     textSize, blurCol);
    dotText(c, label.c_str(), tx + 1, ty,     textSize, blurCol);
    dotText(c, label.c_str(), tx,     ty + 1, textSize, blurCol);
  } else {
    dotText(c, label.c_str(), tx, ty, textSize, textCol);

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
    // scrollDir=+1 (down): items move UP (angles decrease). The item that was
    //   BELOW (next) slides up to center, old selected slides up to top.
    //   New item enters from below. Like scrolling a list down.
    // scrollDir=-1 (up): items move DOWN (angles increase). The item that was
    //   ABOVE (prev) slides down to center, old selected slides down to bottom.
    //   New item enters from above. Like scrolling a list up.
    float shift = -scrollDir * 60.0f * t;

    uint8_t itemIdx[4];
    float baseAngles[4];

    if (scrollDir > 0) {
      // Scrolling down: items slide UP. New selected enters from BELOW.
      // [oldPrev, oldSel, newSel, newNext] at [-60, 0, +60, +120]
      itemIdx[0] = (selectedIndex + count - 2) % count;    // old prev (slides off top)
      itemIdx[1] = (selectedIndex + count - 1) % count;    // old selected (moves to top)
      itemIdx[2] = selectedIndex;                          // new selected (moves to center from below)
      itemIdx[3] = (selectedIndex + 1) % count;            // new next (enters from bottom)
      baseAngles[0] = -60; baseAngles[1] = 0; baseAngles[2] = 60; baseAngles[3] = 120;
    } else {
      // Scrolling up: items slide DOWN. New selected enters from ABOVE.
      // [newPrev, newSel, oldSel, oldNext] at [-120, -60, 0, +60]
      itemIdx[0] = (selectedIndex + count - 1) % count;    // new prev (enters from top)
      itemIdx[1] = selectedIndex;                          // new selected (moves to center from above)
      itemIdx[2] = (selectedIndex + 1) % count;            // old selected (moves to bottom)
      itemIdx[3] = (selectedIndex + 2) % count;            // old next (slides off bottom)
      baseAngles[0] = -120; baseAngles[1] = -60; baseAngles[2] = 0; baseAngles[3] = 60;
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
