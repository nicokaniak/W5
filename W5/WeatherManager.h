#ifndef WEATHERMANAGER_H
#define WEATHERMANAGER_H

#include <Arduino.h>

static const uint8_t HOURLY_FORECAST_HOURS = 24;

struct HourlyForecast {
  int   hour[HOURLY_FORECAST_HOURS];       // 0-23
  float temperature[HOURLY_FORECAST_HOURS]; // °C
  float precipitation[HOURLY_FORECAST_HOURS]; // mm
  int   weatherCode[HOURLY_FORECAST_HOURS]; // open-meteo code
  uint8_t count;                            // valid entries (≤24)
};

class WeatherManager {
public:
  static void initWeather();
  static void updateWeather();
  static String getTemperature();

  static const HourlyForecast& getHourlyForecast();
  static bool hasHourlyData();
};

#endif