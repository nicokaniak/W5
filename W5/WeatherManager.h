#ifndef WEATHERMANAGER_H
#define WEATHERMANAGER_H

#include <Arduino.h>

class WeatherManager {
public:
  static void initWeather();
  static void updateWeather();
  static String getWeatherInfo();
  static String getTemperature();
  static String getWindSpeed();
  static String getWeatherDescription();
};

#endif