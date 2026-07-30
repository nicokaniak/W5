// Hardware diagnostic for LilyGO T-Display-S3-AMOLED battery issues
// This will help identify if the problem is hardware or software

void setup() {
  // Enable power pins FIRST
  pinMode(15, OUTPUT);
  digitalWrite(15, HIGH);
  delay(100);

  pinMode(38, OUTPUT);
  digitalWrite(38, HIGH);
  delay(100);

  Serial.begin(115200);
  delay(1000);

  Serial.println(
      "\n\n=== LilyGO T-Display-S3-AMOLED Hardware Diagnostic ===\n");

  // Setup battery voltage pin
  pinMode(4, INPUT);

  runDiagnostics();
}

void runDiagnostics() {
  Serial.println("Running diagnostics...\n");

  // Test 1: Battery voltage reading
  Serial.println("TEST 1: Battery Voltage Reading");
  Serial.println("--------------------------------");
  float voltage = readBatteryVoltage();
  Serial.print("Battery voltage: ");
  Serial.print(voltage, 3);
  Serial.println("V");

  if (voltage < 0.5) {
    Serial.println("❌ FAIL: No battery detected or bad connection");
    Serial.println("   → Check JST connector is firmly plugged in");
    Serial.println("   → Verify battery is not completely dead");
  } else if (voltage < 3.0) {
    Serial.println("⚠️  WARNING: Battery voltage very low");
    Serial.println("   → Charge battery before testing");
    Serial.println("   → Board may not boot on battery at this voltage");
  } else if (voltage < 3.3) {
    Serial.println("⚠️  WARNING: Battery voltage low");
    Serial.println("   → Charge battery soon");
  } else if (voltage > 4.3) {
    Serial.println("❌ FAIL: Voltage reading too high (calibration issue)");
    Serial.println("   → Check ADC calibration");
  } else {
    Serial.println("✅ PASS: Battery voltage normal");
  }
  Serial.println();

  // Test 2: GPIO15 state
  Serial.println("TEST 2: GPIO15 (Power Enable) State");
  Serial.println("------------------------------------");
  int gpio15State = digitalRead(15);
  Serial.print("GPIO15 state: ");
  Serial.println(gpio15State ? "HIGH" : "LOW");

  if (gpio15State) {
    Serial.println("✅ PASS: GPIO15 is HIGH");
  } else {
    Serial.println(
        "❌ FAIL: GPIO15 is LOW - this will prevent battery operation");
  }
  Serial.println();

  // Test 3: GPIO38 state
  Serial.println("TEST 3: GPIO38 (Backlight Enable) State");
  Serial.println("----------------------------------------");
  int gpio38State = digitalRead(38);
  Serial.print("GPIO38 state: ");
  Serial.println(gpio38State ? "HIGH" : "LOW");

  if (gpio38State) {
    Serial.println("✅ PASS: GPIO38 is HIGH");
  } else {
    Serial.println("❌ FAIL: GPIO38 is LOW - display won't work");
  }
  Serial.println();

  // Test 4: USB power detection
  Serial.println("TEST 4: USB Power Detection");
  Serial.println("---------------------------");
  Serial.println("Currently running on USB power (Serial is working)");
  Serial.println("To test battery power:");
  Serial.println("  1. Note the current battery voltage above");
  Serial.println("  2. Disconnect USB cable");
  Serial.println("  3. Wait 5 seconds");
  Serial.println("  4. Reconnect USB cable");
  Serial.println(
      "  5. Check if voltage reading continues from where it left off");
  Serial.println();

  // Test 5: ADC stability
  Serial.println("TEST 5: ADC Reading Stability");
  Serial.println("------------------------------");
  Serial.println("Taking 10 readings over 2 seconds...");
  float readings[10];
  float sum = 0;
  float minV = 999;
  float maxV = 0;

  for (int i = 0; i < 10; i++) {
    readings[i] = readBatteryVoltage();
    sum += readings[i];
    if (readings[i] < minV)
      minV = readings[i];
    if (readings[i] > maxV)
      maxV = readings[i];
    Serial.print("  Reading ");
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.print(readings[i], 3);
    Serial.println("V");
    delay(200);
  }

  float avg = sum / 10.0;
  float variance = maxV - minV;

  Serial.print("Average: ");
  Serial.print(avg, 3);
  Serial.println("V");
  Serial.print("Variance: ");
  Serial.print(variance, 3);
  Serial.println("V");

  if (variance > 0.2) {
    Serial.println("⚠️  WARNING: High variance in readings");
    Serial.println("   → May indicate poor battery connection");
  } else {
    Serial.println("✅ PASS: Stable readings");
  }
  Serial.println();

  // Summary
  Serial.println("=== DIAGNOSTIC SUMMARY ===");
  Serial.println();
  Serial.println("Next steps:");
  Serial.println("1. If battery voltage is good (>3.3V), try the USB "
                 "disconnect test above");
  Serial.println(
      "2. If board shuts down when USB is disconnected, this indicates:");
  Serial.println("   a) Hardware issue with power management circuit");
  Serial.println("   b) GPIO15 not holding power during transition");
  Serial.println("   c) Possible board defect");
  Serial.println();
  Serial.println("3. If board DOES stay on when USB disconnected:");
  Serial.println("   → Hardware is OK, issue is in main W5 sketch");
  Serial.println("   → Check initialization order in W5.ino");
  Serial.println();
}

float readBatteryVoltage() {
  uint32_t raw = 0;
  for (int i = 0; i < 10; i++) {
    raw += analogRead(4);
    delayMicroseconds(100);
  }
  raw /= 10;

  // Voltage divider is 2:1, reference is 3.3V, ADC is 12-bit (4095)
  float voltage = (raw * 3.3 * 2.0) / 4095.0;
  return voltage;
}

void loop() {
  // Maintain power pins
  digitalWrite(15, HIGH);
  digitalWrite(38, HIGH);

  // Read battery every 5 seconds
  float voltage = readBatteryVoltage();
  int percentage = (int)((voltage - 3.3) / (4.2 - 3.3) * 100);
  if (percentage > 100)
    percentage = 100;
  if (percentage < 0)
    percentage = 0;

  Serial.print("[");
  Serial.print(millis() / 1000);
  Serial.print("s] Battery: ");
  Serial.print(voltage, 2);
  Serial.print("V (");
  Serial.print(percentage);
  Serial.println("%)");

  // Blink backlight to show we're alive (visible without USB)
  digitalWrite(38, LOW);
  delay(50);
  digitalWrite(38, HIGH);

  delay(4950);
}
