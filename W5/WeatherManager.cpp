#include "WeatherManager.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "config/config.h"

static String weatherInfo = "No data";

void WeatherManager::initWeather() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  int retryCount = 0;
  while (WiFi.status() != WL_CONNECTED && retryCount < 20) {
    delay(500);
    Serial.print(".");
    retryCount++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected to WiFi");
  } else {
    Serial.println("WiFi connection failed");
  }
}

void WeatherManager::updateWeather() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String apiUrl = "https://api.open-meteo.com/v1/forecast?latitude=" + String(LATITUDE) + "&longitude=" + String(LONGITUDE) + "&current_weather=true";
    http.begin(apiUrl);
    int httpCode = http.GET();
    if (httpCode == 200) {
      String payload = http.getString();
      int tempIdx = payload.indexOf("\"temperature\":");
      if (tempIdx != -1) {
        int start = tempIdx + 14;
        int end = payload.indexOf(",", start);
        String tempStr = payload.substring(start, end);
        weatherInfo = "Temp: " + tempStr + " °C";
        Serial.println("Weather updated: " + weatherInfo);
      }
    } else {
      Serial.println("Weather API request failed");
    }
    http.end();
  } else {
    Serial.println("WiFi not connected");
  }
}

String WeatherManager::getWeatherInfo() {
  return weatherInfo;
}