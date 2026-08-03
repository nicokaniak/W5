#include "WeatherManager.h"
#include "config/config.h"
#include "DisplayManager.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <time.h>

static String weatherInfo = "No data";
static String temperature = "--";
static String windSpeed = "--";
static String weatherDescription = "Unknown";

static HourlyForecast hourlyForecast;
static bool hourlyValid = false;

static void fetchHourlyForecast();

// Convert weather code to description
String getWeatherCodeDescription(int code) {
  if (code == 0)
    return "Clear sky";
  if (code == 1 || code == 2 || code == 3)
    return "Partly cloudy";
  if (code == 45 || code == 48)
    return "Foggy";
  if (code == 51 || code == 53 || code == 55)
    return "Drizzle";
  if (code == 61 || code == 63 || code == 65)
    return "Rain";
  if (code == 71 || code == 73 || code == 75)
    return "Snow";
  if (code == 77)
    return "Snow grains";
  if (code == 80 || code == 81 || code == 82)
    return "Rain showers";
  if (code == 85 || code == 86)
    return "Snow showers";
  if (code == 95)
    return "Thunderstorm";
  if (code == 96 || code == 99)
    return "Thunderstorm";
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
}

void WeatherManager::updateWeather() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String apiUrl =
        "https://api.open-meteo.com/v1/forecast?latitude=" + String(LATITUDE) +
        "&longitude=" + String(LONGITUDE) + "&current_weather=true";
    http.begin(apiUrl);
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
        temperature = payload.substring(start, end);
        weatherInfo = "Temp: " + temperature + " °C";
        Serial.println("Weather updated: " + weatherInfo);
      }

      // Parse wind speed
      int windIdx = payload.indexOf("\"windspeed\":", cwIdx);
      if (windIdx != -1) {
        int start = windIdx + 12;
        int end = payload.indexOf(",", start);
        windSpeed = payload.substring(start, end);
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
        weatherDescription = getWeatherCodeDescription(code);
      }

    } else {
      Serial.println("Weather API request failed");
      temperature = "Error";
      windSpeed = "Error";
      weatherDescription = "API Error";
    }
    http.end();
  } else {
    Serial.println("WiFi not connected");
    temperature = "No WiFi";
    windSpeed = "No WiFi";
    weatherDescription = "No WiFi";
  }

  fetchHourlyForecast();
}

// ponytail: manual JSON extraction — no ArduinoJson dependency. Finds array
// values by key name, parses floats/ints. Ceiling: if open-meteo changes their
// JSON structure, this breaks. Upgrade path: ArduinoJson.
static float extractFloat(const String &payload, const char *key, int idx) {
  String searchKey = String("\"") + key + "\":[";
  int arrStart = payload.indexOf(searchKey);
  if (arrStart < 0) return 0.0f;
  arrStart += searchKey.length();

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

static int extractInt(const String &payload, const char *key, int idx) {
  return (int)extractFloat(payload, key, idx);
}

static void fetchHourlyForecast() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  // ponytail: hourly forecast — temperature_2m, precipitation, weathercode for next 24h.
  // forecast_hours=24 limits the response size. &past_days=0 starts from current hour.
  String apiUrl =
      "https://api.open-meteo.com/v1/forecast?latitude=" + String(LATITUDE) +
      "&longitude=" + String(LONGITUDE) +
      "&hourly=temperature_2m,precipitation,weathercode&forecast_days=2&timezone=auto";
  http.begin(apiUrl);
  http.setTimeout(8000);
  int httpCode = http.GET();

  if (httpCode != 200) {
    Serial.printf("Hourly forecast HTTP error: %d\n", httpCode);
    http.end();
    return;
  }

  String payload = http.getString();
  http.end();

  // Find the current hour offset by matching against local time.
  // open-meteo returns ISO timestamps like "2024-01-15T14:00".
  // We find the first entry whose hour matches the current local hour.
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 1000)) {
    Serial.println("Hourly: no NTP time, using index 0");
    timeinfo.tm_hour = -1; // fallback: start from beginning
  }

  // Parse the time array to find the starting index matching current hour.
  String timeKey = "\"time\":[";
  int timeStart = payload.indexOf(timeKey);
  if (timeStart < 0) {
    Serial.println("Hourly: time array not found");
    return;
  }
  timeStart += timeKey.length();

  // Find the first timestamp that matches or is after current hour.
  int startIndex = 0;
  int pos = timeStart;
  for (int i = 0; i < 48; i++) {
    int quote1 = payload.indexOf('"', pos);
    int quote2 = payload.indexOf('"', quote1 + 1);
    if (quote1 < 0 || quote2 < 0) break;

    String ts = payload.substring(quote1 + 1, quote2);
    // Extract hour from "YYYY-MM-DDTHH:00"
    int tIdx = ts.indexOf('T');
    if (tIdx >= 0 && tIdx + 3 < (int)ts.length()) {
      int tsHour = ts.substring(tIdx + 1, tIdx + 3).toInt();
      if (tsHour == timeinfo.tm_hour || timeinfo.tm_hour < 0) {
        startIndex = i;
        break;
      }
    }
    pos = quote2 + 1;
    int comma = payload.indexOf(',', pos);
    if (comma < 0) break;
    pos = comma + 1;
  }

  // Extract 24 hours starting from startIndex.
  uint8_t count = 0;
  for (int i = 0; i < HOURLY_FORECAST_HOURS; i++) {
    int dataIdx = startIndex + i;
    float temp = extractFloat(payload, "temperature_2m", dataIdx);
    float precip = extractFloat(payload, "precipitation", dataIdx);
    int wcode = extractInt(payload, "weathercode", dataIdx);

    // Parse hour from timestamp for this entry.
    int hour = 0;
    pos = timeStart;
    bool found = false;
    for (int j = 0; j <= dataIdx; j++) {
      int q1 = payload.indexOf('"', pos);
      int q2 = payload.indexOf('"', q1 + 1);
      if (q1 < 0 || q2 < 0) break;
      if (j == dataIdx) {
        String ts = payload.substring(q1 + 1, q2);
        int tIdx = ts.indexOf('T');
        if (tIdx >= 0 && tIdx + 3 < (int)ts.length())
          hour = ts.substring(tIdx + 1, tIdx + 3).toInt();
        found = true;
        break;
      }
      pos = q2 + 1;
      int comma = payload.indexOf(',', pos);
      if (comma < 0) break;
      pos = comma + 1;
    }

    if (!found) break;

    hourlyForecast.hour[count] = hour;
    hourlyForecast.temperature[count] = temp;
    hourlyForecast.precipitation[count] = precip;
    hourlyForecast.weatherCode[count] = wcode;
    count++;
  }

  hourlyForecast.count = count;
  hourlyValid = count > 0;
  Serial.printf("Hourly forecast: %d entries starting at hour %d\n",
                count, count > 0 ? hourlyForecast.hour[0] : -1);
}

String WeatherManager::getWeatherInfo() { return weatherInfo; }

String WeatherManager::getTemperature() { return temperature; }

String WeatherManager::getWindSpeed() { return windSpeed; }

String WeatherManager::getWeatherDescription() { return weatherDescription; }

const HourlyForecast& WeatherManager::getHourlyForecast() { return hourlyForecast; }

bool WeatherManager::hasHourlyData() { return hourlyValid; }