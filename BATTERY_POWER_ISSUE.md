# Battery Power Issue: Board Shuts Down When USB Disconnected

## Your Specific Problem

**Symptoms:**
- ✅ USB + Battery connected: Works fine
- ✅ USB only (no battery): Shows "battery disconnected" correctly  
- ❌ Battery only (no USB): **Board shuts down completely**

**This indicates:** The power management circuit is not maintaining power from the battery when USB is disconnected.

## Why This Happens

The LilyGO T-Display-S3-AMOLED has a power management IC (likely an AXP2101 or similar) that:

1. **Requires GPIO15 to stay HIGH** to enable battery power to the ESP32
2. **Experiences a brief power glitch** when USB is disconnected
3. **GPIO15 goes LOW** during this glitch (because the ESP32 briefly loses power)
4. **Power IC cuts battery power** when it sees GPIO15 go LOW
5. **Board shuts down** completely

This is a **chicken-and-egg problem**: You need power to keep GPIO15 HIGH, but you need GPIO15 HIGH to get power from the battery.

## Solutions to Try (In Order)

### Solution 1: Hardware Pull-Up Resistor (BEST FIX)

The most reliable solution is to add a **hardware pull-up resistor** to GPIO15:

**What you need:**
- 10kΩ resistor (or anything from 4.7kΩ to 47kΩ)
- Soldering iron
- Thin wire

**How to do it:**
1. Solder one end of the resistor to GPIO15 pin
2. Solder the other end to 3.3V pin
3. This keeps GPIO15 HIGH even during power transitions

**Why this works:**
- The resistor maintains GPIO15 HIGH even when ESP32 briefly loses power
- The power IC sees continuous HIGH signal
- Battery power stays enabled during USB disconnect

### Solution 2: Test with Enhanced Sketch

Try the enhanced battery test sketch I created:

**File:** `battery_test_enhanced.ino`

**What it does:**
- Uses direct register access for faster GPIO15 initialization
- Continuously reinforces GPIO15 HIGH state in loop
- May help if timing is the issue

**How to test:**
1. Open `battery_test_enhanced.ino` in Arduino IDE
2. Upload to board
3. Open Serial Monitor (115200 baud)
4. Watch the boot count
5. Disconnect USB
6. Wait 10 seconds
7. Reconnect USB
8. Check if boot count increased (means board stayed on)

### Solution 3: Check for Hardware Defect

Run the hardware diagnostic:

**File:** `hardware_diagnostic.ino`

**What it checks:**
- Battery voltage and connection
- GPIO15 and GPIO38 states
- ADC reading stability
- Provides specific recommendations

**How to use:**
1. Open `hardware_diagnostic.ino` in Arduino IDE
2. Upload to board
3. Open Serial Monitor (115200 baud)
4. Read the diagnostic results
5. Follow the specific recommendations

### Solution 4: Alternative Power Hold Method

Some users have success with this approach:

```cpp
void setup() {
  // Method 1: Standard approach
  pinMode(15, OUTPUT);
  digitalWrite(15, HIGH);
  
  // Method 2: Enable internal pull-up (may help)
  pinMode(15, INPUT_PULLUP);
  delay(10);
  pinMode(15, OUTPUT);
  digitalWrite(15, HIGH);
  
  // Method 3: Use RTC GPIO hold (survives brief power loss)
  gpio_hold_en((gpio_num_t)15);
  
  // Rest of setup...
}
```

### Solution 5: Check Board Variant

Verify you have the correct board variant:

**Your board should be:**
- LilyGO T-Display-S3-AMOLED (with AMOLED screen)
- **NOT** T-Display-S3 (regular version with IPS screen)

**How to check:**
- AMOLED version has a vibrant, high-contrast screen
- AMOLED version has different power management
- Check the board marking/label

## Testing Procedure

### Step 1: Run Hardware Diagnostic
```
1. Upload hardware_diagnostic.ino
2. Open Serial Monitor
3. Check all tests pass
4. Note the battery voltage
```

### Step 2: Test Enhanced Sketch
```
1. Upload battery_test_enhanced.ino
2. Open Serial Monitor
3. Note the boot count
4. Disconnect USB
5. Wait 10 seconds (watch for backlight blinking)
6. Reconnect USB
7. Check if boot count increased
```

### Step 3: Interpret Results

**If boot count increased:**
- ✅ Board stayed on during USB disconnect
- ✅ Hardware is working
- → Issue is in main W5 sketch initialization

**If boot count did NOT increase:**
- ❌ Board shut down when USB disconnected
- ❌ Hardware issue or power management problem
- → Try Solution 1 (hardware pull-up) or contact support

## Common Issues and Fixes

### Issue: Battery voltage shows 0V or very low
**Fix:** 
- Check JST connector is firmly plugged in
- Measure battery voltage with multimeter
- Charge battery if below 3.3V
- Try different battery

### Issue: Battery voltage normal but board still shuts down
**Fix:**
- Likely power management IC issue
- Try hardware pull-up resistor (Solution 1)
- May be board defect - contact LilyGO support

### Issue: Board works on battery for a few seconds then shuts down
**Fix:**
- Battery may be weak or damaged
- Check battery capacity (should be at least 200mAh)
- Measure battery voltage under load
- Try larger battery (500mAh+)

### Issue: Display stays black even though board is running
**Fix:**
- GPIO38 not set HIGH
- Check backlight enable in code
- May need display initialization

## Hardware Modification (Advanced)

If software solutions don't work, you can modify the board:

### Add Pull-Up Resistor to GPIO15

**Components needed:**
- 10kΩ resistor (0603 SMD or through-hole)
- Soldering iron with fine tip
- Flux

**Steps:**
1. Locate GPIO15 pad on board
2. Locate 3.3V pad (usually near ESP32)
3. Solder resistor between GPIO15 and 3.3V
4. Test with battery

**Caution:** This requires soldering skills. If uncomfortable, contact LilyGO support.

## Expected Battery Life

With your 200mAh battery:
- **Active display:** 1-2 hours
- **Display off:** 4-6 hours  
- **Deep sleep:** 24+ hours

For longer runtime:
- Use 500mAh or 1000mAh battery
- Implement sleep modes
- Reduce display brightness
- Disable WiFi when not needed

## Files Created

1. **battery_test_enhanced.ino** - Enhanced test with register access
2. **hardware_diagnostic.ino** - Comprehensive hardware testing
3. **BATTERY_POWER_ISSUE.md** - This troubleshooting guide

## Next Steps

1. **Start with hardware diagnostic** to identify the issue
2. **Try enhanced battery test** to see if software fix works
3. **If still failing**, likely need hardware pull-up resistor
4. **If all else fails**, contact LilyGO support (may be defective board)

## References

- LilyGO GitHub: https://github.com/Xinyuan-LilyGO/T-Display-S3-AMOLED
- ESP32 GPIO Hold: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/gpio.html
- Power Management: Check your board's schematic for power IC details

## Contact Support

If none of these solutions work:

**LilyGO Support:**
- GitHub Issues: https://github.com/Xinyuan-LilyGO/T-Display-S3-AMOLED/issues
- Email: Check your purchase receipt
- Provide: Board version, battery specs, diagnostic results

**Information to include:**
- Board exact model (T-Display-S3-AMOLED)
- Battery specifications (402030 3.7V 200mAh)
- Diagnostic results from hardware_diagnostic.ino
- Photos of battery connection
- Purchase date and seller
