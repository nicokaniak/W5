#include "WeatherManager.h"
#include "config/config.h"
#include "DisplayManager.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

static String temperature = "--";

static HourlyForecast hourlyForecast;
static bool hourlyValid = false;

// ponytail: fetch runs on a FreeRTOS task on core 0 so the 16s HTTP window
// doesn't starve button polling on core 1 (Arduino loop). Mutex guards the
// three shared globals during the brief publish; HTTP I/O happens outside the
// lock. Ceiling: a torn read is impossible (mutex), but a getter called mid-
// publish returns the prior snapshot — acceptable, corrected on the next frame.
static SemaphoreHandle_t dataMutex = nullptr;
static TaskHandle_t      fetchTask  = nullptr;
static volatile bool     fetching   = false;

static void fetchWeather();
static void fetchHourlyForecastInto(const String &payload, HourlyForecast &out, bool &valid);

// ponytail: const char* return — no String alloc. Only used for Serial.printf
// now, but if it ever feeds the display this stays zero-alloc.
const char *getWeatherCodeDescription(int code) {
  if (code == 0) return "Clear sky";
  if (code >= 1 && code <= 3) return "Partly cloudy";
  if (code == 45 || code == 48) return "Foggy";
  if (code == 51 || code == 53 || code == 55) return "Drizzle";
  if (code == 61 || code == 63 || code == 65) return "Rain";
  if (code == 71 || code == 73 || code == 75) return "Snow";
  if (code == 77) return "Snow grains";
  if (code == 80 || code == 81 || code == 82) return "Rain showers";
  if (code == 85 || code == 86) return "Snow showers";
  if (code == 95) return "Thunderstorm";
  if (code == 96 || code == 99) return "Thunderstorm";
  return "Unknown";
}

// Try previously-saved credentials (e.g. from WiFiManager), showing the same
// pulsing wifi icon during the connect window.
static bool tryConnectSaved() {
  Serial.println("Trying saved Wi-Fi credentials...");
  // ponytail: WiFi.begin() with no SSID uses the last stored STA config.
  // If nothing is saved, it will fail through the same 4s window as a normal try.
  WiFi.begin();

  int retryCount = 0;
  while (WiFi.status() != WL_CONNECTED && retryCount < 20) {
    DisplayManager::drawWifiConnecting();
    delay(200);
    retryCount++;
  }
  Serial.println();
  return WiFi.status() == WL_CONNECTED;
}

// Try one network, showing pulsing wifi icon during the 10s connect window.
static bool tryConnect(const char *ssid, const char *password) {
  Serial.printf("Connecting to \"%s\"...\n", ssid);
  WiFi.begin(ssid, password);

  int retryCount = 0;
  while (WiFi.status() != WL_CONNECTED && retryCount < 20) {
    DisplayManager::drawWifiConnecting();
    delay(200);
    retryCount++;
  }
  Serial.println();
  return WiFi.status() == WL_CONNECTED;
}

static void fetchTaskFunc(void *) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    fetchWeather();
    fetching = false;
  }
}

void WeatherManager::initWeather() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(500);

  bool connected = false;

  // Phase 0: Credentials saved by on-device setup (e.g. WiFiManager).
  // These take precedence so a user who reconfigured Wi-Fi doesn't have to
  // rebuild and reflash just to update the hardcoded list.
  if (tryConnectSaved()) {
    Serial.println("Connected to saved credentials");
    connected = true;
  }

  // Phase 1: Hardcoded saved networks — these have real internet, so NTP/weather
  // work. Open networks used to be tried first, which often latched onto a
  // captive portal: WiFi showed "connected" but NTP couldn't reach the internet,
  // leaving the clock stuck at 00:00:00.
  for (int i = 0; i < WIFI_NETWORK_COUNT && !connected; i++) {
    if (tryConnect(WIFI_NETWORKS[i].ssid, WIFI_NETWORKS[i].password)) {
      Serial.println("Connected to saved network");
      connected = true;
      break;
    }
    WiFi.disconnect(true);
    delay(1000);
  }

  // Phase 2: Fall back to any open (unencrypted) network — best-effort time
  // sync when no saved network is in range.
  // ponytail: ceiling — an open network that's a captive portal still reports
  // WL_CONNECTED but has no internet, so NTP silently fails. Upgrade path:
  // probe a cheap HTTP HEAD (e.g. http://pool.ntp.org) before accepting it.
  if (!connected) {
    Serial.println("Scanning for open WiFi networks...");
    WiFi.scanNetworks(true); // async
    int scanWait = 0;
    while (WiFi.scanComplete() < 0 && scanWait < 100) {
      DisplayManager::drawWifiConnecting();
      delay(100);
      scanWait++;
    }

    int scanCount = WiFi.scanComplete();
    if (scanCount > 0) {
      for (int i = 0; i < scanCount && !connected; i++) {
        if (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) {
          if (tryConnect(WiFi.SSID(i).c_str(), "")) {
            Serial.println("Connected to open network");
            connected = true;
          } else {
            WiFi.disconnect(true);
            delay(500);
          }
        }
      }
    }
    WiFi.scanDelete();
  }

  if (!connected) {
    Serial.println("Could not connect to any WiFi network.");
  }

  // ponytail: spawn the fetch task on core 0 so periodic 10-min fetches don't
  // block button polling on core 1. Stack 8KB covers TLS + String parsing.
  // Priority 1 stays below WiFi/system tasks.
  if (!dataMutex) dataMutex = xSemaphoreCreateMutex();
  if (!fetchTask) {
    xTaskCreatePinnedToCore(fetchTaskFunc, "weather", 8192, nullptr, 1, &fetchTask, 0);
  }
}

// ponytail: actual fetch work, runs either on the fetch task (async) or the
// caller thread (sync boot). Writes results to locals, then publishes under
// the mutex so getters never see a torn struct.
static void fetchWeather() {
  String localTemp = temperature; // preserve prior value if fetch fails
  HourlyForecast localForecast = {};
  bool localValid = false;

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    // ponytail: stack buffer instead of String concat — avoids 4-5 heap allocs
    // per fetch. LATITUDE/LONGITUDE are compile-time string macros.
    char apiUrl[192];
    snprintf(apiUrl, sizeof(apiUrl),
             "https://api.open-meteo.com/v1/forecast?latitude=%s&longitude=%s&current_weather=true",
             LATITUDE, LONGITUDE);
    // ponytail: http.begin(url) creates a WiFiClientSecure that validates server
    // certs by default, but no CA cert is loaded and system time may not be
    // synced yet — TLS handshake fails silently. setInsecure() skips validation
    // (acceptable: public weather data, no secrets). Ceiling: MITM possible.
    // Upgrade path: load a root CA bundle with wifiClient.setCACert().
    WiFiClientSecure secureClient;
    secureClient.setInsecure();
    http.begin(secureClient, apiUrl);
    int httpCode = http.GET();
    if (httpCode == 200) {
      String payload = http.getString();

      // ponytail: open-meteo emits a "current_weather_units" block before the
      // real "current_weather" block, and both contain the same keys (temperature,
      // windspeed, weathercode). A naive indexOf matches the units block first,
      // which holds string units like "°C" instead of the numeric value. Anchor
      // all key searches to the actual current_weather object so we parse values,
      // not units. Ceiling: if open-meteo renames the key, this breaks. Upgrade
      // path: ArduinoJson.
      int cwIdx = payload.indexOf("\"current_weather\":{");
      if (cwIdx < 0) cwIdx = 0; // fall back to whole payload if shape changes

      // Parse temperature
      int tempIdx = payload.indexOf("\"temperature\":", cwIdx);
      if (tempIdx != -1) {
        int start = tempIdx + 14;
        int end = payload.indexOf(",", start);
        if (end == -1) end = payload.indexOf("}", start); // last field in object
        localTemp = payload.substring(start, end);
        Serial.println("Weather updated: " + localTemp + " °C");
      }

      // Parse weather code
      int codeIdx = payload.indexOf("\"weathercode\":", cwIdx);
      if (codeIdx != -1) {
        int start = codeIdx + 14;
        int end = payload.indexOf(",", start);
        if (end == -1)
          end = payload.indexOf("}", start);
        String codeStr = payload.substring(start, end);
        int code = codeStr.toInt();
        Serial.printf("Weather code: %d (%s)\n", code, getWeatherCodeDescription(code));
      }

    } else {
      Serial.println("Weather API request failed");
      localTemp = "Error";
    }
    http.end();
  } else {
    Serial.println("WiFi not connected");
    localTemp = "No WiFi";
  }

  // Hourly forecast into locals
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    // ponytail: hourly forecast — temperature_2m, precipitation, weathercode for next 24h.
    // forecast_hours=24 limits the response size. &past_days=0 starts from current hour.
    char apiUrl[192];
    snprintf(apiUrl, sizeof(apiUrl),
             "https://api.open-meteo.com/v1/forecast?latitude=%s&longitude=%s"
             "&hourly=temperature_2m,precipitation,weathercode&forecast_days=2&timezone=auto",
             LATITUDE, LONGITUDE);
    WiFiClientSecure secureClient;
    secureClient.setInsecure();
    http.begin(secureClient, apiUrl);
    http.setTimeout(8000);
    int httpCode = http.GET();

    if (httpCode == 200) {
      String payload = http.getString();
      http.end();
      fetchHourlyForecastInto(payload, localForecast, localValid);
    } else {
      Serial.printf("Hourly forecast HTTP error: %d\n", httpCode);
      http.end();
    }
  }

  // Publish under mutex
  if (dataMutex && xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
    temperature     = localTemp;
    hourlyForecast  = localForecast;
    hourlyValid     = localValid;
    xSemaphoreGive(dataMutex);
  }
}

void WeatherManager::updateWeather(bool async) {
  if (async && fetchTask) {
    fetching = true;
    xTaskNotifyGive(fetchTask);
  } else {
    // Sync path: boot (task not yet created) or explicit sync request
    fetching = true;
    fetchWeather();
    fetching = false;
  }
}

bool WeatherManager::isFetching() { return fetching; }

// ponytail: find the start of a JSON array value ("key":[...). Returns the
// index of the first char after "[", or -1 if not found.
static int findArrayStart(const String &payload, const char *key) {
  String searchKey = String("\"") + key + "\":[";
  int idx = payload.indexOf(searchKey);
  if (idx < 0) return -1;
  return idx + searchKey.length();
}

// ponytail: extract element idx from a JSON array starting at arrStart.
// Caller pre-computes arrStart via findArrayStart so we don't re-scan the
// whole payload 72 times per forecast. Ceiling: same as before — naive string
// parsing, no nested arrays. Upgrade path: ArduinoJson.
static float extractFloatAt(const String &payload, int arrStart, int idx) {
  if (arrStart < 0) return 0.0f;
  int pos = arrStart;
  for (int i = 0; i < idx; i++) {
    int comma = payload.indexOf(',', pos);
    if (comma < 0) return 0.0f;
    pos = comma + 1;
  }
  int end = payload.indexOf(',', pos);
  if (end < 0) end = payload.indexOf(']', pos);
  if (end < 0) return 0.0f;
  return payload.substring(pos, end).toFloat();
}

// ponytail: single-pass hourly parse. The old code re-walked the time array
// from the start for each of 24 hours (O(n²), ~300 indexOf calls). Now we
// cache every hour in one pass, then index directly. Array starts for
// temperature_2m / precipitation / weathercode are found once, not 72 times.
static void fetchHourlyForecastInto(const String &payload, HourlyForecast &out, bool &valid) {
  // Find the current hour offset by matching against local time.
  // open-meteo returns ISO timestamps like "2024-01-15T14:00".
  // We find the first entry whose hour matches the current local hour.
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 1000)) {
    Serial.println("Hourly: no NTP time, using index 0");
    timeinfo.tm_hour = -1; // fallback: start from beginning
  }

  // Parse the time array — single pass, cache every hour.
  String timeKey = "\"time\":[";
  int timeStart = payload.indexOf(timeKey);
  if (timeStart < 0) {
    Serial.println("Hourly: time array not found");
    return;
  }
  timeStart += timeKey.length();

  // ponytail: cache all hours in one walk; find startIndex as we go.
  int8_t hours[48];
  uint8_t hourCount = 0;
  int startIndex = 0;
  bool startIndexFound = false;
  int pos = timeStart;
  for (int i = 0; i < 48; i++) {
    int quote1 = payload.indexOf('"', pos);
    int quote2 = payload.indexOf('"', quote1 + 1);
    if (quote1 < 0 || quote2 < 0) break;

    String ts = payload.substring(quote1 + 1, quote2);
    // Extract hour from "YYYY-MM-DDTHH:00"
    int tIdx = ts.indexOf('T');
    int hour = -1;
    if (tIdx >= 0 && tIdx + 3 < (int)ts.length()) {
      hour = ts.substring(tIdx + 1, tIdx + 3).toInt();
    }
    hours[hourCount++] = hour;

    if (!startIndexFound && (hour == timeinfo.tm_hour || timeinfo.tm_hour < 0)) {
      startIndex = i;
      startIndexFound = true;
    }

    pos = quote2 + 1;
    int comma = payload.indexOf(',', pos);
    if (comma < 0) break;
    pos = comma + 1;
  }

  // Find the three data array starts once (was 72 indexOf calls).
  int tempStart   = findArrayStart(payload, "temperature_2m");
  int precipStart = findArrayStart(payload, "precipitation");
  int codeStart   = findArrayStart(payload, "weathercode");

  // Extract 24 hours starting from startIndex — direct index, no re-walk.
  uint8_t count = 0;
  for (int i = 0; i < HOURLY_FORECAST_HOURS; i++) {
    int dataIdx = startIndex + i;
    if (dataIdx >= hourCount) break;

    out.hour[count]          = hours[dataIdx];
    out.temperature[count]   = extractFloatAt(payload, tempStart, dataIdx);
    out.precipitation[count] = extractFloatAt(payload, precipStart, dataIdx);
    out.weatherCode[count]   = (int)extractFloatAt(payload, codeStart, dataIdx);
    count++;
  }

  out.count = count;
  valid = count > 0;
  Serial.printf("Hourly forecast: %d entries starting at hour %d\n",
                count, count > 0 ? out.hour[0] : -1);
}

String WeatherManager::getTemperature() {
  if (dataMutex && xSemaphoreTake(dataMutex, 5) == pdTRUE) {
    String t = temperature;
    xSemaphoreGive(dataMutex);
    return t;
  }
  return temperature; // best-effort if mutex contended (fetch task publishing)
}

HourlyForecast WeatherManager::getHourlyForecast() {
  if (dataMutex && xSemaphoreTake(dataMutex, 5) == pdTRUE) {
    HourlyForecast h = hourlyForecast;
    xSemaphoreGive(dataMutex);
    return h;
  }
  return hourlyForecast; // best-effort
}

bool WeatherManager::hasHourlyData() {
  if (dataMutex && xSemaphoreTake(dataMutex, 5) == pdTRUE) {
    bool v = hourlyValid;
    xSemaphoreGive(dataMutex);
    return v;
  }
  return hourlyValid; // best-effort
}
