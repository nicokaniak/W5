#include "TimeManager.h"
#include "DisplayManager.h"

#include "pins_config.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <time.h>

static String currentTime = "00:00:00";

// IANA timezone name → POSIX TZ string. Covers major world zones.
// Falls back to TIMEZONE from pins_config.h if the API-fetched zone isn't here.
struct TzMap { const char *iana; const char *posix; };

static const TzMap TZ_MAP[] = {
  {"UTC", "UTC0"},
  // Europe — CET/CEST (UTC+1 winter, UTC+2 summer)
  {"Europe/Copenhagen", "CET-1CEST-2,M3.5.0/3,M10.5.0/4"},
  {"Europe/Paris",       "CET-1CEST-2,M3.5.0/3,M10.5.0/4"},
  {"Europe/Berlin",      "CET-1CEST-2,M3.5.0/3,M10.5.0/4"},
  {"Europe/Madrid",      "CET-1CEST-2,M3.5.0/3,M10.5.0/4"},
  {"Europe/Rome",        "CET-1CEST-2,M3.5.0/3,M10.5.0/4"},
  {"Europe/Amsterdam",   "CET-1CEST-2,M3.5.0/3,M10.5.0/4"},
  {"Europe/Brussels",    "CET-1CEST-2,M3.5.0/3,M10.5.0/4"},
  {"Europe/Stockholm",   "CET-1CEST-2,M3.5.0/3,M10.5.0/4"},
  {"Europe/Oslo",        "CET-1CEST-2,M3.5.0/3,M10.5.0/4"},
  {"Europe/Zurich",      "CET-1CEST-2,M3.5.0/3,M10.5.0/4"},
  {"Europe/Vienna",      "CET-1CEST-2,M3.5.0/3,M10.5.0/4"},
  {"Europe/Warsaw",      "CET-1CEST-2,M3.5.0/3,M10.5.0/4"},
  {"Europe/Prague",      "CET-1CEST-2,M3.5.0/3,M10.5.0/4"},
  {"Europe/Budapest",    "CET-1CEST-2,M3.5.0/3,M10.5.0/4"},
  {"Europe/Lisbon",      "WET0WEST-1,M3.5.0/3,M10.5.0/4"},
  {"Europe/London",      "GMT0BST-1,M3.5.0/3,M10.5.0/4"},
  // Europe — EET/EEST (UTC+2 winter, UTC+3 summer)
  {"Europe/Helsinki",    "EET-2EEST-3,M3.5.0/3,M10.5.0/4"},
  {"Europe/Athens",      "EET-2EEST-3,M3.5.0/3,M10.5.0/4"},
  {"Europe/Bucharest",   "EET-2EEST-3,M3.5.0/3,M10.5.0/4"},
  {"Europe/Sofia",       "EET-2EEST-3,M3.5.0/3,M10.5.0/4"},
  {"Europe/Istanbul",    "TRT-3"},
  {"Europe/Moscow",      "MSK-3"},
  // Americas
  {"America/New_York",    "EST5EDT,M3.5.0/3,M10.5.0/4"},
  {"America/Toronto",     "EST5EDT,M3.5.0/3,M10.5.0/4"},
  {"America/Chicago",     "CST6CDT,M3.5.0/3,M10.5.0/4"},
  {"America/Mexico_City", "CST6"},
  {"America/Denver",      "MST7MDT,M3.5.0/3,M10.5.0/4"},
  {"America/Phoenix",     "MST7"},
  {"America/Los_Angeles", "PST8PDT,M3.5.0/3,M10.5.0/4"},
  {"America/Vancouver",   "PST8PDT,M3.5.0/3,M10.5.0/4"},
  {"America/Anchorage",   "AKST9AKDT,M3.5.0/3,M10.5.0/4"},
  {"America/Halifax",     "AST4ADT,M3.5.0/3,M10.5.0/4"},
  {"America/Sao_Paulo",   "BRT3"},
  {"America/Buenos_Aires","ART3"},
  {"America/Bogota",      "COT5"},
  {"America/Lima",        "PET5"},
  // Asia
  {"Asia/Tokyo",    "JST-9"},
  {"Asia/Seoul",    "KST-9"},
  {"Asia/Shanghai", "CST-8"},
  {"Asia/Hong_Kong","HKT-8"},
  {"Asia/Singapore","SGT-8"},
  {"Asia/Taipei",   "CST-8"},
  {"Asia/Bangkok",  "ICT-7"},
  {"Asia/Jakarta",  "WIB-7"},
  {"Asia/Kolkata",  "IST-5:30"},
  {"Asia/Dubai",    "GST-4"},
  {"Asia/Tehran",   "IRST-3:30"},
  {"Asia/Jerusalem","IST-2IDT-3,M3.4.5/2,M10.5.0/2"},
  {"Asia/Riyadh",   "AST-3"},
  {"Asia/Karachi",  "PKT-5"},
  {"Asia/Dhaka",    "BST-6"},
  // Australia
  {"Australia/Sydney",   "AEST-10AEDT-11,M10.1.0/3,M4.1.0/3"},
  {"Australia/Melbourne","AEST-10AEDT-11,M10.1.0/3,M4.1.0/3"},
  {"Australia/Brisbane", "AEST-10"},
  {"Australia/Perth",    "AWST-8"},
  {"Australia/Adelaide", "ACST-9:30ACDT-10:30,M10.1.0/3,M4.1.0/3"},
  // Pacific
  {"Pacific/Auckland", "NZST-12NZDT-13,M10.1.0/3,M4.1.0/3"},
  {"Pacific/Honolulu",  "HST10"},
  // Africa
  {"Africa/Cairo",       "EET-2"},
  {"Africa/Johannesburg", "SAST-2"},
  {"Africa/Lagos",       "WAT-1"},
  {"Africa/Nairobi",     "EAT-3"},
  {"Africa/Casablanca",  "WET0WEST-1,M3.5.0/3,M10.5.0/4"},
};

static const int TZ_MAP_COUNT = sizeof(TZ_MAP) / sizeof(TZ_MAP[0]);

static const char *lookupTzPosix(const String &iana) {
  for (int i = 0; i < TZ_MAP_COUNT; i++) {
    if (iana == TZ_MAP[i].iana) return TZ_MAP[i].posix;
  }
  return nullptr;
}

// Fetch IANA timezone from IP geolocation, return POSIX TZ string or nullptr.
static const char *fetchTimezonePosix() {
  if (WiFi.status() != WL_CONNECTED) return nullptr;

  HTTPClient http;
  http.begin(GET_TIMEZONE_API);
  http.setTimeout(5000);
  int code = http.GET();
  if (code != 200) {
    Serial.printf("TZ API returned %d\n", code);
    http.end();
    return nullptr;
  }
  String iana = http.getString();
  http.end();
  iana.trim();
  Serial.printf("IP geolocation TZ: %s\n", iana.c_str());

  const char *posix = lookupTzPosix(iana);
  if (posix) {
    Serial.printf("POSIX TZ: %s\n", posix);
    return posix;
  }
  Serial.printf("TZ '%s' not in lookup table, using fallback\n", iana.c_str());
  return nullptr;
}

void TimeManager::initTime() {
  configTime(0, 0, NTP_SERVER1, NTP_SERVER2);

  // Try IP geolocation for timezone, fall back to hardcoded TIMEZONE.
  // NTP syncs in the background during the HTTP fetch, so no extra boot delay.
  const char *tzPosix = fetchTimezonePosix();
  if (!tzPosix) tzPosix = TIMEZONE;

  // Set TZ AFTER configTime — configTime(0,0,...) resets TZ to UTC internally.
  setenv("TZ", tzPosix, 1);
  tzset();

  // Block up to 15s waiting for NTP sync so the watch face doesn't
  // start at 00:00:00. If WiFi has no internet, we give up and move on.
  Serial.println("Waiting for NTP sync...");
  struct tm timeinfo;
  for (int i = 0; i < 30; i++) {
    if (getLocalTime(&timeinfo, 500)) {
      Serial.printf("NTP synced: %04d-%02d-%02d %02d:%02d:%02d\n",
                    timeinfo.tm_year + 1900, timeinfo.tm_mon + 1,
                    timeinfo.tm_mday, timeinfo.tm_hour,
                    timeinfo.tm_min, timeinfo.tm_sec);
      return;
    }
    Serial.print(".");
  }
  Serial.println("\nNTP sync failed — no internet or NTP unreachable");
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
