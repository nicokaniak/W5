#ifndef MENUMANAGER_H
#define MENUMANAGER_H

#include <Arduino.h>

enum MenuScreen {
  WATCH_FACE,
  WEATHER_SCREEN,
  ALARMS_SCREEN,
  BATTERY_SCREEN,
  BLUETOOTH_SCREEN,
  MENU_SCREEN_COUNT // Total number of screens
};

class MenuManager {
public:
  static void initMenu();
  static void handleButtons();
  static MenuScreen getCurrentScreen();
  static void setScreen(MenuScreen screen);

private:
  static MenuScreen currentScreen;
  static unsigned long lastButtonPress;
  static const unsigned long DEBOUNCE_DELAY = 200; // ms
  static bool button1LastState;
  static bool button2LastState;
};

#endif
