#include "WeatherManager.h"
#include "config/config.h"
#include <HTTPClient.h>
#include <WiFi.h>

static String weatherInfo = "No data";

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

String WeatherManager::getWeatherInfo() { return weatherInfo; }