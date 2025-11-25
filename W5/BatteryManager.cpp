#include "BatteryManager.h"
#include "pins_config.h"

void BatteryManager::initBattery() {
  // Enable power for battery operation (required for LilyGO T-Display AMOLED)
  // BOTH pins are required for battery operation:

  // GPIO15 enables the power circuit for display and battery
  pinMode(PIN_POWER_ON, OUTPUT);
  digitalWrite(PIN_POWER_ON, HIGH);

  // GPIO38 enables the backlight
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);

  pinMode(PIN_BAT_VOLT, INPUT);
}

float BatteryManager::getVoltage() {
  // Read ADC value (0-4095)
  // The voltage divider is typically 100k + 100k, so we multiply by 2.
  // Reference voltage is 3.3V.
  // Formula: ADC * 3.3 * 2 / 4095
  // Note: You might need to calibrate the multiplier (e.g. 2.0 or slightly
  // different)

  uint32_t raw = 0;
  for (int i = 0; i < 10; i++) {
    raw += analogRead(PIN_BAT_VOLT);
    delay(2);
  }
  raw /= 10;

  float voltage = (raw * 3.3 * 2.0) / 4095.0;
  return voltage;
}

int BatteryManager::getPercentage() {
  float voltage = getVoltage();
  // Simple estimation for LiPo
  // 4.2V = 100%, 3.3V = 0%
  int percentage = (int)((voltage - 3.3) / (4.2 - 3.3) * 100);

  if (percentage > 100)
    percentage = 100;
  if (percentage < 0)
    percentage = 0;

  return percentage;
}
