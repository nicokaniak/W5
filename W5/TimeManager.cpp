#include "TimeManager.h"
#include "DisplayManager.h"

#include "pins_config.h"
#include <time.h>
#include <sys/time.h>

static String currentTime = "00:00:00";

void TimeManager::initTime() {
  // Initialize NTP
  configTime(GMT_OFFSET_SEC, DAY_LIGHT_OFFSET_SEC, NTP_SERVER1, NTP_SERVER2);
}

// ponytail: phone pushes local wall-clock. GMT_OFFSET_SEC=0 so getLocalTime
// returns the RTC as-is. configTime(0,0,...) leaves TZ=UTC, so mktime here
// treats the struct tm as UTC and the epoch we store equals the phone's local
// clock — which then displays verbatim.
// Ceiling: if NTP resyncs over WiFi later, it overwrites with UTC. Upgrade
// path: store a TZ offset and apply it in getLocalTime.
void TimeManager::setLocalTime(int year, int month, int day, int hour, int minute, int second) {
  struct tm t = {};
  t.tm_year = year - 1900;
  t.tm_mon  = month - 1;
  t.tm_mday = day;
  t.tm_hour = hour;
  t.tm_min  = minute;
  t.tm_sec  = second;
  t.tm_isdst = 0;
  time_t epoch = mktime(&t);
  if (epoch < 0) {
    Serial.println("setLocalTime: mktime failed");
    return;
  }
  struct timeval tv = {};
  tv.tv_sec = epoch;
  settimeofday(&tv, nullptr);
  Serial.printf("BLE time set: %04d-%02d-%02d %02d:%02d:%02d\n",
                year, month, day, hour, minute, second);
}

void TimeManager::updateTime() {
  struct tm timeinfo;
  // Try to get time with a short timeout so we don't block the loop too long if
  // not synced
  if (!getLocalTime(&timeinfo, 10)) {
    return;
  }

  char timeStringBuff[50];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%H:%M:%S", &timeinfo);
  currentTime = String(timeStringBuff);
}

String TimeManager::getCurrentTime() { return currentTime; }

void TimeManager::drawWatchFace(const String &timeStr) {
  DisplayManager::clearDisplay();
  DisplayManager::drawText(timeStr, 10, 30);
}
