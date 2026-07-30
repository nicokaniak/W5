# LilyGO T-Display-S3-AMOLED Battery Power Troubleshooting Guide

## Problem
Board does not run on battery power when USB is disconnected.

## Solution Summary
The T-Display-S3-AMOLED requires TWO GPIO pins to be enabled for battery operation:
- **GPIO15**: Enables the power circuit for display and battery
- **GPIO38**: Enables the display backlight

## Critical Requirements

### 1. Pin Initialization Order
Power pins MUST be enabled BEFORE any other initialization, including Serial.begin():

```cpp
void setup() {
  // FIRST: Enable GPIO15 (power circuit)
  pinMode(15, OUTPUT);
  digitalWrite(15, HIGH);
  delay(100);
  
  // SECOND: Enable GPIO38 (backlight)
  pinMode(38, OUTPUT);
  digitalWrite(38, HIGH);
  delay(100);
  
  // NOW you can initialize everything else
  Serial.begin(115200);
  // ... rest of setup
}
```

### 2. Stabilization Delays
Add 100ms delays after each pin to allow power circuits to stabilize.

### 3. Battery Specifications
Your 402030 3.7V 200mAh LiPo battery is compatible:
- Voltage range: 3.3V (empty) to 4.2V (full)
- Connection: JST connector on board

## Testing Steps

### Step 1: Upload Test Sketch
1. Open `battery_test.ino` in Arduino IDE
2. Upload to board via USB
3. Open Serial Monitor (115200 baud)
4. Verify you see "Battery Power Test" messages

### Step 2: Test Battery Operation
1. Keep Serial Monitor open
2. **Disconnect USB cable**
3. If working: Board should continue running (you won't see serial output without USB)
4. Reconnect USB and check if board was running

### Step 3: Visual Indicators
When running on battery:
- Display backlight should blink every second (in test sketch)
- Board should feel warm (normal operation)

## Common Issues

### Issue 1: Board Still Won't Run on Battery
**Possible causes:**
- Battery not properly connected to JST connector
- Battery voltage too low (< 3.3V) - charge the battery
- Faulty battery or connector
- Hardware defect

**Solutions:**
- Check battery connection is secure
- Measure battery voltage with multimeter (should be 3.7V - 4.2V)
- Try a different battery
- Check JST connector for damage

### Issue 2: Display Not Turning On
**Possible causes:**
- GPIO38 not set HIGH
- Display initialization happens before power enable

**Solutions:**
- Verify GPIO38 is set HIGH in first lines of setup()
- Add delays after power pin initialization
- Check DisplayManager::initDisplay() is called AFTER power pins

### Issue 3: Battery Drains Quickly
**Normal behavior:**
- AMOLED display uses significant power
- 200mAh battery will last ~1-2 hours with active display
- WiFi operations drain battery faster

**Solutions:**
- Use larger battery (500mAh - 1000mAh)
- Implement sleep modes when not in use
- Reduce display brightness
- Disable WiFi when not needed

## Hardware Verification

### Check Battery Voltage
```cpp
// Read battery voltage on GPIO4
uint32_t raw = analogRead(4);
float voltage = (raw * 3.3 * 2.0) / 4095.0;
Serial.print("Battery: ");
Serial.print(voltage);
Serial.println("V");
```

Expected values:
- 4.2V = Fully charged
- 3.7V = Normal
- 3.3V = Low (charge soon)
- < 3.0V = Too low (won't boot)

### Verify Power Pins
Use a multimeter to check:
- GPIO15 should be 3.3V when set HIGH
- GPIO38 should be 3.3V when set HIGH
- Battery voltage at JST connector should be 3.7V - 4.2V

## Files Modified

### W5.ino
- Power pins enabled as FIRST thing in setup()
- Added stabilization delays
- Removed BatteryManager::initBattery() call (now done inline)

### BatteryManager.cpp
- Still has power enable code (for reference)
- getVoltage() and getPercentage() functions unchanged

### battery_test.ino (NEW)
- Minimal test sketch
- Verifies power pins are working
- Blinks backlight to show operation

## References
- Official LilyGO repo: https://github.com/Xinyuan-LilyGO/T-Display-S3-AMOLED
- Community examples: https://github.com/teastainGit/LilyGO-T-display-S3-setup-and-examples
- GPIO15 documentation: LCD and battery power enable
- GPIO38 documentation: Backlight enable pin

## Next Steps if Still Not Working

1. **Test with minimal sketch**: Upload battery_test.ino first
2. **Check hardware**: Verify battery voltage and connections
3. **Try different battery**: Rule out battery issues
4. **Contact support**: May be hardware defect
5. **Check board version**: Ensure you have T-Display-S3-AMOLED (not regular T-Display-S3)

## Success Indicators
✅ Board continues running after USB disconnect
✅ Display remains lit on battery
✅ Battery voltage reading is accurate (3.7V - 4.2V)
✅ Serial messages appear when USB reconnected
