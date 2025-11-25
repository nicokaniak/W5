#include "TimeManager.h"
#include "DisplayManager.h"

static String currentTime = "00:00:00";

void TimeManager::initTime() {
  // Initialize RTC or NTP sync here
}

void TimeManager::updateTime() {
  // Update currentTime string here, e.g., from RTC or NTP
  currentTime = "12:34:56"; // Example placeholder
}

String TimeManager::getCurrentTime() {
  return currentTime;
}

void TimeManager::drawWatchFace(const String &timeStr) {
  DisplayManager::clearDisplay();
  DisplayManager::drawText(timeStr, 10, 30);
}
