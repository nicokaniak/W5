#include "WeatherManager.h"
#include "config/config.h"
#include <HTTPClient.h>
#include <WiFi.h>

static String weatherInfo = "No data";
static String temperature = "--";
static String windSpeed = "--";
static String weatherDescription = "Unknown";

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

void WeatherManager::initWeather() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(1000);

  for (int i = 0; i < WIFI_NETWORK_COUNT; i++) {
    Serial.print("Connecting to ");
    Serial.println(WIFI_NETWORKS[i].ssid);

    WiFi.begin(WIFI_NETWORKS[i].ssid, WIFI_NETWORKS[i].password);

    int retryCount = 0;
    while (WiFi.status() != WL_CONNECTED && retryCount < 20) {
      delay(500);
      Serial.print(".");
      retryCount++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Connected to WiFi");
      return;
    } else {
      Serial.print("Failed to connect. Status: ");
      Serial.println(WiFi.status());
      WiFi.disconnect(true);
      delay(2000);
    }
  }
  Serial.println("Could not connect to any WiFi network.");
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

      // Parse temperature
      int tempIdx = payload.indexOf("\"temperature\":");
      if (tempIdx != -1) {
        int start = tempIdx + 14;
        int end = payload.indexOf(",", start);
        temperature = payload.substring(start, end);
        weatherInfo = "Temp: " + temperature + " °C";
        Serial.println("Weather updated: " + weatherInfo);
      }

      // Parse wind speed
      int windIdx = payload.indexOf("\"windspeed\":");
      if (windIdx != -1) {
        int start = windIdx + 12;
        int end = payload.indexOf(",", start);
        windSpeed = payload.substring(start, end);
      }

      // Parse weather code
      int codeIdx = payload.indexOf("\"weathercode\":");
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
}

String WeatherManager::getWeatherInfo() { return weatherInfo; }

String WeatherManager::getTemperature() { return temperature; }

String WeatherManager::getWindSpeed() { return windSpeed; }

String WeatherManager::getWeatherDescription() { return weatherDescription; }