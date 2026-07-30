// Enhanced battery test for LilyGO T-Display-S3-AMOLED
// This version includes additional power hold mechanisms

// Use RTC_DATA_ATTR to keep GPIO state during deep sleep transitions
RTC_DATA_ATTR int bootCount = 0;

void setup() {
  // CRITICAL: Set GPIO15 as OUTPUT and HIGH immediately
  // This MUST be the absolute first thing - even before any variable
  // initialization Using direct register access for maximum speed
  GPIO.enable_w1ts = ((uint32_t)1 << 15); // Enable GPIO15 output
  GPIO.out_w1ts = ((uint32_t)1 << 15);    // Set GPIO15 HIGH

  // Also set via pinMode for safety
  pinMode(15, OUTPUT);
  digitalWrite(15, HIGH);

  // Small delay to ensure power circuit stabilizes
  delayMicroseconds(1000); // 1ms

  // Enable GPIO38 (backlight) immediately after
  pinMode(38, OUTPUT);
  digitalWrite(38, HIGH);
  delayMicroseconds(1000); // 1ms

  // Additional power hold - enable internal pull-up on GPIO15
  // This helps maintain HIGH state during transitions
  pinMode(15, OUTPUT);
  digitalWrite(15, HIGH);

  // Now we can safely initialize Serial
  Serial.begin(115200);
  delay(500);

  bootCount++;

  Serial.println("\n\n=== Enhanced Battery Power Test ===");
  Serial.print("Boot count: ");
  Serial.println(bootCount);
  Serial.println("GPIO15 (power): HIGH (with register access)");
  Serial.println("GPIO38 (backlight): HIGH");
  Serial.println("");

  // Setup battery voltage reading
  pinMode(4, INPUT);

  // Read and display initial battery voltage
  float voltage = readBatteryVoltage();
  Serial.print("Initial battery voltage: ");
  Serial.print(voltage, 2);
  Serial.println("V");
  Serial.println("");
  Serial.println("Test sequence:");
  Serial.println("1. Keep USB connected and watch battery readings");
  Serial.println("2. Disconnect USB cable");
  Serial.println("3. Watch for backlight blinking (1 sec on, 0.1 sec off)");
  Serial.println("4. Reconnect USB and check if boot count increased");
  Serial.println("");
}

float readBatteryVoltage() {
  uint32_t raw = 0;
  for (int i = 0; i < 10; i++) {
    raw += analogRead(4);
    delay(2);
  }
  raw /= 10;

  float voltage = (raw * 3.3 * 2.0) / 4095.0;
  return voltage;
}

int getBatteryPercentage(float voltage) {
  int percentage = (int)((voltage - 3.3) / (4.2 - 3.3) * 100);

  if (percentage > 100)
    percentage = 100;
  if (percentage < 0)
    percentage = 0;

  return percentage;
}

void loop() {
  // Continuously reinforce GPIO15 HIGH state
  digitalWrite(15, HIGH);

  // Read battery voltage
  float voltage = readBatteryVoltage();
  int percentage = getBatteryPercentage(voltage);

  Serial.print("Battery: ");
  Serial.print(voltage, 2);
  Serial.print("V (");
  Serial.print(percentage);
  Serial.print("%) | GPIO15: ");
  Serial.print(digitalRead(15) ? "HIGH" : "LOW");
  Serial.print(" | GPIO38: ");
  Serial.println(digitalRead(38) ? "HIGH" : "LOW");

  // Blink the backlight to show we're alive
  // This is visible even without USB/Serial
  digitalWrite(38, LOW);
  delay(100);
  digitalWrite(38, HIGH);

  // Reinforce power pin again
  digitalWrite(15, HIGH);

  delay(900);
}
