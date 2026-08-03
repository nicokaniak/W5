#include "DisplayManager.h"
#include "BatteryManager.h"
#include "BluetoothManager.h"
#include "RM67162Display.h"
#include "MenuManager.h"
#include "StopwatchManager.h"
#include "TimeManager.h"
#include "WeatherManager.h"
#include "config/config.h" // LATITUDE / LONGITUDE for Dusk2Dawn
#include "Dusk2Dawn.h"
#include "moonPhaser.h"
#include "starfield_icons.h" // 7-seg digits, moon phases, am/pm, wifi (from watchy-starfield)
#include "battery_icons.h" // 1-bit battery state icons (generated from icons/battery/*.png)
#include "icons.h" // 1-bit PROGMEM menu icons (generated from icons/*.png)
#include "weather_icons.h" // 1-bit PROGMEM weather icons (generated from icons/weather/*.png)
#include "rm67162.h" // For lcd_PushColors
#include <math.h>
#include <WiFi.h>

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

// ----- Starfield watch face helpers -----
// Adapted from watchy-starfield-main: 7-seg digit bitmaps, moon phase,
// sunrise/sunset, procedural starfield background. Layout is landscape
// (536x240) instead of the original 200x200 portrait e-paper.

static moonPhaser s_moonP;

// ponytail: 16-bit LCG for a deterministic starfield. Same seed → same stars
// every frame, so the background is stable across redraws. Ceiling: stars
// are fixed at boot; no drift/twinkle. Upgrade: animate with millis().
static uint32_t s_starSeed = 0x1234ABCD;
static inline uint32_t lcg16() {
  s_starSeed = s_starSeed * 1103515245u + 12345u;
  return (s_starSeed >> 16) & 0x7FFF;
}

// Draw n deterministic stars onto the canvas. Sizes vary 1-2px for depth.
static void drawStarfield(GFXcanvas16 *c, int n) {
  s_starSeed = 0x1234ABCD; // reset for determinism
  for (int i = 0; i < n; i++) {
    int16_t x = (int16_t)(lcg16() % c->width());
    int16_t y = (int16_t)(lcg16() % c->height());
    uint8_t  r = (lcg16() & 1) ? 2 : 1;
    // Dim white/blue stars for depth — RGB565
    uint16_t col = (lcg16() & 3) == 0 ? 0xBDF7 : 0xFFFF; // 25% bluish, rest white
    if (r == 1)
      c->drawPixel(x, y, col);
    else
      c->fillCircle(x, y, 1, col);
  }
}

// 7-seg big digit (33x53) lookup. fd_0..fd_9 from starfield_icons.h.
static const unsigned char *fdDigits[] = {
  fd_0, fd_1, fd_2, fd_3, fd_4, fd_5, fd_6, fd_7, fd_8, fd_9
};
// 7-seg small digit (16x25) lookup. dd_0..dd_9
static const unsigned char *ddDigits[] = {
  dd_0, dd_1, dd_2, dd_3, dd_4, dd_5, dd_6, dd_7, dd_8, dd_9
};
// 3x5 tiny digit lookup. num_0..num_9
static const unsigned char *numDigits[] = {
  num_0, num_1, num_2, num_3, num_4, num_5, num_6, num_7, num_8, num_9
};

// Draw a multi-digit 7-seg number at (x,y) using big 33x53 bitmaps.
// Returns the total width drawn.
static int drawBigDigits(GFXcanvas16 *c, int value, int nDigits,
                         int16_t x, int16_t y, uint16_t color) {
  const int dw = 33, dh = 53, gap = 4;
  for (int i = nDigits - 1; i >= 0; i--) {
    int d = value % 10;
    value /= 10;
    int16_t dx = x + i * (dw + gap);
    c->drawBitmap(dx, y, fdDigits[d], dw, dh, color);
  }
  return nDigits * (dw + gap) - gap;
}

// Draw a multi-digit 7-seg number at (x,y) using small 16x25 bitmaps.
static int drawSmallDigits(GFXcanvas16 *c, int value, int nDigits,
                           int16_t x, int16_t y, uint16_t color) {
  const int dw = 16, dh = 25, gap = 3;
  for (int i = nDigits - 1; i >= 0; i--) {
    int d = value % 10;
    value /= 10;
    int16_t dx = x + i * (dw + gap);
    c->drawBitmap(dx, y, ddDigits[d], dw, dh, color);
  }
  return nDigits * (dw + gap) - gap;
}

// Draw a 3x5 tiny digit at (x,y) — used for sunrise/sunset HH:MM.
static void drawTinyDigits(GFXcanvas16 *c, int value, int nDigits,
                           int16_t x, int16_t y, uint16_t color) {
  const int dw = 3, dh = 5, gap = 1;
  for (int i = nDigits - 1; i >= 0; i--) {
    int d = value % 10;
    value /= 10;
    int16_t dx = x + i * (dw + gap);
    c->drawBitmap(dx, y, numDigits[d], dw, dh, color);
  }
}

// Draw a 1-bit PROGMEM bitmap scaled up by integer `scale`. Used to make the
// 33x53 7-seg digits fill the top half of the screen. fillRect per set pixel.
static void drawBitmapScaled(GFXcanvas16 *c, int16_t x, int16_t y,
                             const unsigned char *bm, int16_t w, int16_t h,
                             int scale, uint16_t color) {
  int16_t bytesPerRow = (w + 7) / 8;
  for (int16_t row = 0; row < h; row++) {
    for (int16_t col = 0; col < w; col++) {
      uint16_t byteIdx = row * bytesPerRow + col / 8;
      uint8_t bits = pgm_read_byte(&bm[byteIdx]);
      if (bits & (0x80 >> (col & 7))) {
        c->fillRect(x + col * scale, y + row * scale, scale, scale, color);
      }
    }
  }
}

// Draw n 33x53 7-seg digits scaled by `scale`, with `gap` px between digits.
static void drawBigDigitsScaled(GFXcanvas16 *c, int value, int nDigits,
                                int16_t x, int16_t y, int scale, int gap,
                                uint16_t color) {
  const int dw = 33, dh = 53;
  int step = dw * scale + gap;
  for (int i = nDigits - 1; i >= 0; i--) {
    int d = value % 10;
    value /= 10;
    drawBitmapScaled(c, x + i * step, y, fdDigits[d], dw, dh, scale, color);
  }
}

// Pick the moon phase bitmap based on angle (0-360) and percentLit (0-1).
// Waxing (0-180): luna1..luna7,12. Waning (180-360): luna1..luna7,12.
static const unsigned char *moonBitmap(int angle, double lit) {
  // luna1 = new, luna7 = full, luna12 = waning crescent
  // Waxing: 1(new) → 12 → 11 → 10 → 9 → 8 → 7(full)
  // Waning: 1(new) → 2 → 3 → 4 → 5 → 6 → 7(full)
  if (angle <= 180) {
    if (lit < 0.1)  return luna1;
    if (lit < 0.25) return luna12;
    if (lit < 0.4)  return luna11;
    if (lit < 0.6)  return luna10;
    if (lit < 0.75) return luna9;
    if (lit < 0.9)  return luna8;
    return luna7;
  }
  if (lit < 0.1)  return luna1;
  if (lit < 0.25) return luna2;
  if (lit < 0.4)  return luna3;
  if (lit < 0.6)  return luna4;
  if (lit < 0.75) return luna5;
  if (lit < 0.9)  return luna6;
  return luna7;
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

  // ----- Starfield watch face (adapted from watchy-starfield-main) -----
  // Layout (536x240 landscape):
  //   Left two-thirds (0-360), split horizontally:
  //     Top half  (0-120):   Big 7-seg clock, scaled to fill
  //     Bottom half (120-240): Date (DOW + DD MON YYYY)
  //   Right third (360-536): Moon phase, sunrise/sunset arc, wifi, temp
  //   Battery icon (32x32): top-right corner, left of the moon

  canvas->fillScreen(0x0000); // black background
  drawStarfield(canvas, 60);   // deterministic procedural stars

  // Fetch local time once for all date/moon/sun calculations below.
  struct tm timeinfo;
  bool haveTime = getLocalTime(&timeinfo, 10);

  // --- Parse time string "HH:MM:SS" ---
  int hh = timeStr.substring(0, 2).toInt();
  int mm = timeStr.substring(3, 5).toInt();

  // --- Top section: big scaled 7-seg clock ---
  // Digit native 33x53. Scale 2 → 66x106. Top section ~120px tall, fits.
  // 4 digits + colon + gaps. Width = 4*66 + colon(16) + 2*gap(12) = 304.
  const uint16_t TIME_COLOR = 0x07FF; // cyan
  const int SCALE = 2;
  const int DW = 33 * SCALE;  // 66
  const int DH = 53 * SCALE;  // 106
  const int DGAP = 12;
  const int COLON_W = 16;
  int clockW = 4 * DW + COLON_W + 2 * DGAP;  // 304
  const int16_t leftSpan = 360;
  int16_t timeX = (leftSpan - clockW) / 2;     // centered in left 2/3
  int16_t timeY = (120 - DH) / 2;             // centered in top half
  if (timeY < 2) timeY = 2;

  // HH
  drawBigDigitsScaled(canvas, hh, 2, timeX, timeY, SCALE, DGAP, TIME_COLOR);
  // Colon (two dots) between HH and MM
  int16_t colonX = timeX + 2 * DW + DGAP + COLON_W / 2;
  int16_t colonY = timeY + DH / 2;
  canvas->fillCircle(colonX, colonY - 10, 4, TIME_COLOR);
  canvas->fillCircle(colonX, colonY + 10, 4, TIME_COLOR);
  // MM
  drawBigDigitsScaled(canvas, mm, 2, timeX + 2 * DW + DGAP + COLON_W + DGAP,
                      timeY, SCALE, DGAP, TIME_COLOR);

  // --- Bottom section: date ---
  const uint16_t DATE_COLOR = 0xBDF7; // light blue-white
  const uint16_t LABEL_COLOR = 0x7BEF; // gray
  const int16_t dateY = 140; // below the midline

  String dateStr = TimeManager::getCurrentDate(); // "Mon 25/12"

  // Day of week (dot text, centered in left 2/3)
  if (dateStr.length() >= 3) {
    String dow = dateStr.substring(0, 3); // "Mon"
    uint16_t dowW, dowH;
    dotTextBounds(dow.c_str(), 3, &dowW, &dowH);
    dotText(canvas, dow.c_str(), (leftSpan - dowW) / 2, dateY, 3, DATE_COLOR);
  }

  // Parse day and month from "Mon 25/12"
  int dayNum = 0, monthNum = 0;
  int slashIdx = dateStr.indexOf('/');
  if (slashIdx > 0) {
    int spaceIdx = dateStr.indexOf(' ');
    if (spaceIdx >= 0) {
      dayNum = dateStr.substring(spaceIdx + 1, slashIdx).toInt();
      monthNum = dateStr.substring(slashIdx + 1).toInt();
    }
  }

  // Day . Month as small 7-seg digits (16x25), centered
  if (dayNum > 0) {
    const int SDW = 16, SDH = 25, SGAP = 4, DOTW = 6;
    int dateRowW = 2 * SDW + DOTW + 2 * SDW + SGAP + SGAP; // DD . MM with gaps
    int16_t dx = (leftSpan - dateRowW) / 2;
    int16_t dy = dateY + 30;
    drawSmallDigits(canvas, dayNum, 2, dx, dy, DATE_COLOR);
    // separator dot
    int16_t dotX = dx + 2 * (SDW + 3) + SGAP / 2;
    canvas->fillCircle(dotX, dy + SDH / 2 - 3, 2, DATE_COLOR);
    canvas->fillCircle(dotX, dy + SDH / 2 + 3, 2, DATE_COLOR);
    drawSmallDigits(canvas, monthNum, 2, dx + 2 * (SDW + 3) + DOTW + SGAP, dy,
                    DATE_COLOR);
  }

  // Year (4 small digits) below day/month
  int year = haveTime ? timeinfo.tm_year + 1900 : 2025;
  {
    const int SDW = 16, SGAP = 3;
    int yearW = 4 * SDW + 3 * SGAP;
    int16_t yx = (leftSpan - yearW) / 2;
    drawSmallDigits(canvas, year, 4, yx, dateY + 60, DATE_COLOR);
  }

  // --- Battery icon (32x32) top-right corner, left of moon ---
  int batPct = BatteryManager::getPercentage();
  uint8_t batIw = 0, batIh = 0;
  const unsigned char *batIcon = batteryIconBitmap(batPct, &batIw, &batIh);
  uint16_t batCol;
  if (batPct > 50)      batCol = 0x07E0; // green
  else if (batPct > 20) batCol = 0xFFE0; // yellow
  else                 batCol = 0xF800; // red
  if (batIcon) {
    // Far top-right corner of the screen, 4px margin.
    canvas->drawBitmap(canvas->width() - batIw - 4, 4, batIcon, batIw, batIh,
                       batCol);
  }

  // --- Right column: moon phase + sunrise/sunset + wifi + temp ---
  const int16_t rightX = 380;

  // Moon phase (61x61, top of right column)
  moonData_t moon;
  int mYear = haveTime ? timeinfo.tm_year + 1900 : 2025;
  int32_t mMonth = haveTime ? timeinfo.tm_mon + 1 : 1;
  int32_t mDay = haveTime ? timeinfo.tm_mday : 1;
  double mHour = haveTime ? timeinfo.tm_hour + 0.1 : 12.0;
  moon = s_moonP.getPhase(mYear, mMonth, mDay, mHour);
  const unsigned char *moonBm = moonBitmap(moon.angle, moon.percentLit);
  canvas->drawBitmap(rightX, 5, moonBm, 61, 61, 0xFFFF); // white moon

  // Sunrise/sunset via Dusk2Dawn
  float lat = String(LATITUDE).toFloat();
  float lon = String(LONGITUDE).toFloat();
  float tzOffset = 0;
  if (haveTime) {
    time_t now = mktime(&timeinfo);
    struct tm utc;
    gmtime_r(&now, &utc);
    tzOffset = (float)(timeinfo.tm_hour - utc.tm_hour) +
               (float)(timeinfo.tm_min - utc.tm_min) / 60.0f;
    if (tzOffset > 12)  tzOffset -= 24;
    if (tzOffset < -12) tzOffset += 24;
  }

  Dusk2Dawn d2d(lat, lon, tzOffset);
  int sr = -1, ss = -1;
  if (haveTime) {
    sr = d2d.sunrise(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1,
                     timeinfo.tm_mday, false);
    ss = d2d.sunset(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1,
                    timeinfo.tm_mday, false);
  }

  const uint16_t SUN_COLOR = 0xFD20; // orange
  const uint16_t SUN_LABEL_COLOR = 0xBDF7; // light blue-white

  if (sr >= 0 && ss >= 0) {
    int nowMin = timeinfo.tm_hour * 60 + timeinfo.tm_min;
    int arcX = rightX + 5;
    int arcY = 75;
    int arcW = 120;
    int tk = (nowMin - sr) * 60 / (ss - sr);
    if (nowMin > ss) tk = 60;
    else if (nowMin < sr) tk = 0;
    canvas->drawLine(arcX, arcY + 30, arcX + arcW, arcY + 30, 0x4208);
    int arrowX = arcX + (arcW * tk) / 60;
    canvas->drawBitmap(arrowX - 1, arcY + 25, arr, 3, 5, SUN_COLOR);

    int srH = sr / 60, srM = sr % 60;
    int ssH = ss / 60, ssM = ss % 60;

    dotText(canvas, "SR", arcX, arcY + 40, 1, LABEL_COLOR);
    drawTinyDigits(canvas, srH, 2, arcX + 14, arcY + 40, SUN_LABEL_COLOR);
    canvas->drawPixel(arcX + 14 + 2 * (3 + 1), arcY + 43, SUN_LABEL_COLOR);
    canvas->drawPixel(arcX + 14 + 2 * (3 + 1), arcY + 45, SUN_LABEL_COLOR);
    drawTinyDigits(canvas, srM, 2, arcX + 14 + 2 * (3 + 1) + 2,
                   arcY + 40, SUN_LABEL_COLOR);

    dotText(canvas, "SS", arcX + arcW - 50, arcY + 40, 1, LABEL_COLOR);
    drawTinyDigits(canvas, ssH, 2, arcX + arcW - 50 + 14, arcY + 40,
                   SUN_LABEL_COLOR);
    canvas->drawPixel(arcX + arcW - 50 + 14 + 2 * (3 + 1), arcY + 43,
                       SUN_LABEL_COLOR);
    canvas->drawPixel(arcX + arcW - 50 + 14 + 2 * (3 + 1), arcY + 45,
                       SUN_LABEL_COLOR);
    drawTinyDigits(canvas, ssM, 2,
                   arcX + arcW - 50 + 14 + 2 * (3 + 1) + 2, arcY + 40,
                   SUN_LABEL_COLOR);
  } else {
    dotText(canvas, "Sun: N/A", rightX, 80, 1, LABEL_COLOR);
  }

  // Temperature (yellow, below sun arc)
  const uint16_t TEMP_COLOR = 0xFFE0;
  String tempStr = WeatherManager::getTemperature();
  bool tempValid = tempStr.length() > 0 && isDigit(tempStr.charAt(0));
  if (tempValid) {
    dotText(canvas, "T:", rightX, 130, 2, LABEL_COLOR);
    int dotPos = tempStr.indexOf('.');
    int tempInt = (dotPos > 0) ? tempStr.substring(0, dotPos).toInt()
                                : tempStr.toInt();
    if (tempInt < 0) {
      dotText(canvas, "-", rightX + 24, 130, 2, TEMP_COLOR);
      drawSmallDigits(canvas, -tempInt, 2, rightX + 44, 130, TEMP_COLOR);
    } else {
      drawSmallDigits(canvas, tempInt, 2, rightX + 24, 130, TEMP_COLOR);
    }
    dotText(canvas, "C", rightX + 2 * (16 + 3) + 44, 130, 2, TEMP_COLOR);
  } else {
    dotText(canvas, "T:--", rightX, 130, 2, LABEL_COLOR);
  }

  // WiFi status indicator (bottom-right)
  bool wifiOn = (WiFi.status() == WL_CONNECTED);
  canvas->drawBitmap(rightX + 50, 175, wifiOn ? wifi : wifioff, 25, 18,
                     wifiOn ? 0x07E0 : 0x7BEF);

  // Push buffer to display
  pushToDisplay();
}

void DisplayManager::drawWeatherScreen() {
  if (!canvas)
    return;

  canvas->fillScreen(0x0000);

  // Display is 536 wide x 240 tall (landscape).
  // Layout:
  //   0-30:    Title + current temp + weather icon
  //  35-165:   Graph area (temperature line + precipitation fill)
  // 170-178:   Hour labels under graph
  // 185-235:   Legend + min/max + location (single row, 536px wide)

  // --- Title ---
  dotText(canvas, "WEATHER", 10, 5, 3, 0xFFE0); // yellow, size 3

  if (!WeatherManager::hasHourlyData()) {
    dotText(canvas, "Fetching forecast...", 10, 40, 2, 0x7BEF);
    dotText(canvas, "Updates every 10 min", 10, 65, 1, 0x7BEF);
    pushToDisplay();
    return;
  }

  const HourlyForecast &fc = WeatherManager::getHourlyForecast();
  uint8_t n = fc.count;
  if (n == 0) {
    dotText(canvas, "No data", 10, 40, 2, 0xF800);
    pushToDisplay();
    return;
  }

  // --- Current weather icon (top-right) ---
  uint8_t iw = 0, ih = 0;
  const unsigned char *icon = weatherIconBitmap(fc.weatherCode[0], &iw, &ih);
  if (icon) {
    canvas->drawBitmap(canvas->width() - iw - 10, 5, icon, iw, ih, 0xFFFF);
  }

  // --- Current temperature text (below title) ---
  String currTemp = String((int)round(fc.temperature[0]));
  dotText(canvas, (currTemp + "C").c_str(), 140, 12, 2, 0xFFFF); // white, size 2

  // --- Graph area ---
  // ponytail: graph leaves 34px on the left for the temperature ruler labels.
  // Temperature is a red line, precipitation is a light-blue filled area.
  // Both share the same x-axis (hours). Y-axes are independent.
  const int16_t rulerW = 24;
  const int16_t graphX = 10 + rulerW;           // 34
  const int16_t graphW = canvas->width() - 20 - rulerW;  // 492px
  const int16_t graphY = 35;
  const int16_t graphH = 130;
  const int16_t graphBottom = graphY + graphH;  // 165

  // Guard against single data point (division by zero in x mapping)
  if (n < 2) {
    int16_t x = graphX + graphW / 2;
    int16_t y = graphY + graphH / 2;
    canvas->fillCircle(x, y, 3, 0xF800);
    pushToDisplay();
    return;
  }

  // Find min/max for temperature scaling
  float tempMin = fc.temperature[0], tempMax = fc.temperature[0];
  float precipMax = 0.0f;
  for (uint8_t i = 0; i < n; i++) {
    if (fc.temperature[i] < tempMin) tempMin = fc.temperature[i];
    if (fc.temperature[i] > tempMax) tempMax = fc.temperature[i];
    if (fc.precipitation[i] > precipMax) precipMax = fc.precipitation[i];
  }
  // Pad temp range so the line doesn't touch the edges
  if (tempMax - tempMin < 2.0f) { tempMin -= 1.0f; tempMax += 1.0f; }
  tempMin -= 1.0f;
  tempMax += 1.0f;
  if (precipMax < 0.1f) precipMax = 1.0f; // avoid div-by-zero, show flat baseline

  // --- Precipitation area (light blue fill) ---
  // ponytail: filled area chart using vertical strips per data point.
  // RGB565 light blue: 0x4B5F (approx #29B5FE)
  const uint16_t PRECIP_COLOR = 0x4B5F;

  for (uint8_t i = 0; i < n; i++) {
    int16_t x = graphX + (int16_t)(graphW * i) / (n - 1);
    int16_t xNext = (i + 1 < n) ? graphX + (int16_t)(graphW * (i + 1)) / (n - 1) : x + 1;

    float precipH = (fc.precipitation[i] / precipMax) * (graphH * 0.5f);
    int16_t barH = (int16_t)precipH;

    // Draw filled bar from bottom up
    if (barH > 0) {
      canvas->fillRect(x, graphBottom - barH, xNext - x, barH, PRECIP_COLOR);
    }
  }

  // --- Temperature line (red) ---
  // ponytail: connected line graph. RGB565 red: 0xF800
  const uint16_t TEMP_COLOR = 0xF800;

  auto tempY = [&](float t) -> int16_t {
    float frac = (t - tempMin) / (tempMax - tempMin);
    return graphBottom - (int16_t)(frac * graphH);
  };

  for (uint8_t i = 1; i < n; i++) {
    int16_t x0 = graphX + (int16_t)(graphW * (i - 1)) / (n - 1);
    int16_t y0 = tempY(fc.temperature[i - 1]);
    int16_t x1 = graphX + (int16_t)(graphW * i) / (n - 1);
    int16_t y1 = tempY(fc.temperature[i]);
    canvas->drawLine(x0, y0, x1, y1, TEMP_COLOR);
  }

  // Draw data points as small circles
  for (uint8_t i = 0; i < n; i++) {
    int16_t x = graphX + (int16_t)(graphW * i) / (n - 1);
    int16_t y = tempY(fc.temperature[i]);
    canvas->fillCircle(x, y, 2, TEMP_COLOR);
  }

  // --- Graph border ---
  canvas->drawRect(graphX, graphY, graphW, graphH, 0x4208); // dark gray border

  // --- Temperature ruler (left side) ---
  // ponytail: 3 tick marks + labels (max, mid, min) aligned to the graph's y-scale.
  // Shows the actual temp range so you can read values off the red line.
  const uint16_t RULER_COLOR = 0x8410; // dark gray
  int tempHi = (int)round(tempMax - 1); // undo the pad
  int tempLo = (int)round(tempMin + 1);
  int tempMid = (tempHi + tempLo) / 2;

  struct { int16_t y; int val; } ticks[] = {
    { graphY,              tempHi },
    { graphY + graphH / 2, tempMid },
    { graphBottom,        tempLo },
  };
  for (auto &t : ticks) {
    // tick mark on the left edge of the graph
    canvas->drawLine(graphX - 4, t.y, graphX, t.y, RULER_COLOR);
    // label just left of the tick
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", t.val);
    dotText(canvas, buf, graphX - 22, t.y - 4, 1, 0x7BEF);
  }

  // --- Hour labels under graph ---
  // ponytail: show every 3rd hour to fit. Size 1 dot text.
  const int16_t labelY = graphBottom + 5;
  for (uint8_t i = 0; i < n; i += 3) {
    int16_t x = graphX + (int16_t)(graphW * i) / (n - 1);
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d", fc.hour[i]);
    // Center the 2-char label on the data point
    dotText(canvas, buf, x - 6, labelY, 1, 0x7BEF); // gray, size 1
  }

  // --- Legend + location (single row at y=185) ---
  const int16_t legendY = 185;
  // Precipitation swatch
  canvas->fillRect(10, legendY, 12, 12, PRECIP_COLOR);
  dotText(canvas, "Rain", 28, legendY - 2, 1, 0x7BEF);

  // Temperature swatch (line)
  canvas->drawLine(90, legendY + 6, 102, legendY + 6, TEMP_COLOR);
  canvas->fillCircle(96, legendY + 6, 2, TEMP_COLOR);
  dotText(canvas, "Temp", 108, legendY - 2, 1, 0x7BEF);

  // --- Location (right-aligned) ---
  dotText(canvas, "Copenhagen", canvas->width() - 90, legendY - 2, 1, 0x7BEF);

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

void DisplayManager::drawConfigMenu(uint8_t selectedIndex) {
  if (!canvas)
    return;

  canvas->fillScreen(0x0000);

  // Title
  dotText(canvas, "CONFIG", 10, 10, 3, 0xFFE0); // yellow

  const uint8_t count = MenuManager::configItemCount();
  // ponytail: hard-coded vertical list spacing. Works for the current 1–3 items;
  // beyond that the list overflows the 240px height and needs scrolling.
  const int16_t startY = 80;
  const int16_t itemH  = 55;

  for (uint8_t i = 0; i < count; i++) {
    int16_t y = startY + i * itemH;
    const char* label = MenuManager::configItemLabel(i);
    bool sel = (i == selectedIndex);
    uint16_t col = sel ? 0x07FF : 0x7BEF;
    uint16_t bulletCol = sel ? 0x07FF : 0x4208;

    // Bullet
    canvas->fillCircle(15, y + 12, 5, bulletCol);

    // Label
    dotText(canvas, label, 35, y, 3, col);

    // Icon for the Wi-Fi setup item; uses the same Wi-Fi bitmaps as the status screen.
    if (i == 0) {
      uint8_t iw = 0, ih = 0;
      const unsigned char* bm = menuIconBitmap(4, sel ? 48 : 32, &iw, &ih);
      if (bm) {
        int16_t ix = canvas->width() - iw - 10;
        int16_t iy = y + (itemH - ih) / 2;
        canvas->drawBitmap(ix, iy, bm, iw, ih, col);
      }
    }
  }

  pushToDisplay();
}

void DisplayManager::drawWifiPortalScreen() {
  if (!canvas)
    return;

  canvas->fillScreen(0x0000);

  // Title
  const char* title = "WIFI SETUP";
  uint16_t w, h;
  dotTextBounds(title, 3, &w, &h);
  dotText(canvas, title, (canvas->width() - w) / 2, 15, 3, 0x07FF); // cyan

  // Wi-Fi icon
  int16_t iconX = (canvas->width() - 48) / 2;
  int16_t iconY = 70;
  canvas->drawBitmap(iconX, iconY, ICON_WIFI_48, 48, 48, 0x07FF);

  // Instructions
  const char* line1 = "Connect to: W5-Setup";
  const char* line2 = "Open 192.168.4.1";
  const char* line3 = "Enter Wi-Fi details";

  dotTextBounds(line1, 2, &w, &h);
  dotText(canvas, line1, (canvas->width() - w) / 2, iconY + 48 + 20, 2, 0xFFFF);
  dotTextBounds(line2, 2, &w, &h);
  dotText(canvas, line2, (canvas->width() - w) / 2, iconY + 48 + 45, 2, 0xFFFF);
  dotTextBounds(line3, 2, &w, &h);
  dotText(canvas, line3, (canvas->width() - w) / 2, iconY + 48 + 70, 2, 0x7BEF);

  pushToDisplay();
}

void DisplayManager::drawWifiResultScreen(bool connected, const String &message) {
  if (!canvas)
    return;

  canvas->fillScreen(0x0000);

  // Title
  const char* title = "WIFI SETUP";
  uint16_t w, h;
  dotTextBounds(title, 3, &w, &h);
  dotText(canvas, title, (canvas->width() - w) / 2, 15, 3, 0x07FF);

  // Status icon
  int16_t iconX = (canvas->width() - 48) / 2;
  int16_t iconY = 60;
  uint16_t iconCol = connected ? 0x07E0 : 0xF800;
  canvas->drawBitmap(iconX, iconY, ICON_WIFI_48, 48, 48, iconCol);

  // Status word
  const char* status = connected ? "Connected" : "Failed";
  dotTextBounds(status, 3, &w, &h);
  dotText(canvas, status, (canvas->width() - w) / 2, iconY + 48 + 15, 3,
          iconCol);

  // Message (truncated to screen width)
  String msg = message;
  if (msg.length() > 32) {
    msg = msg.substring(0, 29) + "...";
  }
  dotTextBounds(msg.c_str(), 2, &w, &h);
  dotText(canvas, msg.c_str(), (canvas->width() - w) / 2, iconY + 48 + 55, 2,
          0xFFFF);

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
