#include "MenuManager.h"
#include "pins_config.h"

// Initialize static members
MenuScreen MenuManager::currentScreen = WATCH_FACE;
unsigned long MenuManager::lastButtonPress = 0;
bool MenuManager::button1LastState = HIGH;
bool MenuManager::button2LastState = HIGH;

void MenuManager::initMenu() {
  // Initialize buttons with internal pull-up resistors
  pinMode(PIN_BUTTON_1, INPUT_PULLUP);
  pinMode(PIN_BUTTON_2, INPUT_PULLUP);

  Serial.println("MenuManager initialized");
  Serial.println("Button 1 (GPIO 0): Next screen");
  Serial.println("Button 2 (GPIO 21): Reserved for future use");
}

void MenuManager::handleButtons() {
  unsigned long currentTime = millis();

  // Read button states (LOW when pressed due to pull-up)
  bool button1State = digitalRead(PIN_BUTTON_1);
  bool button2State = digitalRead(PIN_BUTTON_2);

  // Check if enough time has passed since last button press (debouncing)
  if (currentTime - lastButtonPress < DEBOUNCE_DELAY) {
    return;
  }

  // Button 1: Navigate to next screen
  if (button1State == LOW && button1LastState == HIGH) {
    lastButtonPress = currentTime;

    // Move to next screen
    currentScreen = (MenuScreen)((currentScreen + 1) % MENU_SCREEN_COUNT);

    Serial.print("Screen changed to: ");
    switch (currentScreen) {
    case WATCH_FACE:
      Serial.println("WATCH_FACE");
      break;
    case WEATHER_SCREEN:
      Serial.println("WEATHER_SCREEN");
      break;
    case ALARMS_SCREEN:
      Serial.println("ALARMS_SCREEN");
      break;
    case BATTERY_SCREEN:
      Serial.println("BATTERY_SCREEN");
      break;
    case BLUETOOTH_SCREEN:
      Serial.println("BLUETOOTH_SCREEN");
      break;
    default:
      Serial.println("UNKNOWN");
      break;
    }
  }

  // Button 2: Reserved for future use (select/back functionality)
  if (button2State == LOW && button2LastState == HIGH) {
    lastButtonPress = currentTime;
    Serial.println("Button 2 pressed (reserved for future use)");
  }

  // Update last states
  button1LastState = button1State;
  button2LastState = button2State;
}

MenuScreen MenuManager::getCurrentScreen() { return currentScreen; }

void MenuManager::setScreen(MenuScreen screen) { currentScreen = screen; }
