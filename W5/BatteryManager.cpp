#include "BatteryManager.h"
#include "pins_config.h"

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

bool BatteryManager::isUsbPowerConnected() {
  // ponytail: T-Display-S3 AMOLED has no VBUS GPIO and no charger IC with I2C
  // status. USB-Serial-JTAG only detects USB host (PC) via SOF packets — dumb
  // chargers don't send SOF. The ADC on GPIO4 reads ~4.7V when USB power is
  // present vs ~3.3-4.2V on battery alone. analogReadMilliVolts() uses eFuse
  // calibration (~±2%) so a 4.4V threshold cleanly separates the two.
  // Hysteresis (4.4V rise / 4.25V fall) prevents flickering near the boundary.
  // Ceiling: a deeply uncalibrated ADC could false-trigger; upgrade: wire a
  // GPIO to VBUS through a voltage divider.
  static bool usbConnected = false;
  int mv = analogReadMilliVolts(PIN_BAT_VOLT) * 2; // 2x voltage divider
  if (!usbConnected && mv > 4400) usbConnected = true;
  if (usbConnected && mv < 4250) usbConnected = false;
  return usbConnected;
}
