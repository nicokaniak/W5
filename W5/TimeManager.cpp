#include "TimeManager.h"
#include "DisplayManager.h"

#include "pins_config.h"
#include <time.h>

static String currentTime = "00:00:00";

void TimeManager::initTime() {
  // Initialize NTP
  configTime(GMT_OFFSET_SEC, DAY_LIGHT_OFFSET_SEC, NTP_SERVER1, NTP_SERVER2);
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
