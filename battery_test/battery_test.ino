// Minimal test sketch for LilyGO T-Display-S3-AMOLED battery operation
// This is a bare-bones test to verify power pins are working

void setup() {
  // CRITICAL: Enable power pins FIRST - before anything else!
  // GPIO15 = Power circuit enable
  pinMode(15, OUTPUT);
  digitalWrite(15, HIGH);
  delay(200);

  // GPIO38 = Backlight enable
  pinMode(38, OUTPUT);
  digitalWrite(38, HIGH);
  delay(200);

  // Now initialize Serial
  Serial.begin(115200);
  delay(1000);

  Serial.println("=== Battery Power Test ===");
  Serial.println("GPIO15 (power): HIGH");
  Serial.println("GPIO38 (backlight): HIGH");
  Serial.println("");
  Serial.println("If you see this message after disconnecting USB,");
  Serial.println("the battery power is working!");
  Serial.println("");

  // Setup battery voltage reading
  pinMode(4, INPUT);
}

void loop() {
  // Read battery voltage
  uint32_t raw = 0;
  for (int i = 0; i < 10; i++) {
    raw += analogRead(4);
    delay(2);
  }
  raw /= 10;

  float voltage = (raw * 3.3 * 2.0) / 4095.0;
  int percentage = (int)((voltage - 3.3) / (4.2 - 3.3) * 100);

  if (percentage > 100)
    percentage = 100;
  if (percentage < 0)
    percentage = 0;

  Serial.print("Battery: ");
  Serial.print(voltage, 2);
  Serial.print("V (");
  Serial.print(percentage);
  Serial.println("%)");

  // Blink the backlight to show we're alive
  digitalWrite(38, LOW);
  delay(100);
  digitalWrite(38, HIGH);
  delay(900);
}
