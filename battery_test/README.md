# Battery Test Sketch

## Purpose
This is a minimal test sketch to verify that your LilyGO T-Display-S3-AMOLED can run on battery power.

## How to Use

### Step 1: Open the Test Sketch
1. In Arduino IDE, go to **File → Open**
2. Navigate to: `C:\Users\MashMakesMothershipP\Documents\Arduino\W5\battery_test\`
3. Open `battery_test.ino`

### Step 2: Upload to Board
1. Connect your board via USB
2. Select the correct board and port in Arduino IDE
3. Click **Upload**
4. Wait for upload to complete

### Step 3: Test Battery Operation
1. Open **Serial Monitor** (Tools → Serial Monitor or Ctrl+Shift+M)
2. Set baud rate to **115200**
3. You should see:
   ```
   === Battery Power Test ===
   GPIO15 (power): HIGH
   GPIO38 (backlight): HIGH
   
   If you see this message after disconnecting USB,
   the battery power is working!
   
   Battery: 3.85V (61%)
   ```

4. **Disconnect the USB cable**
5. Watch the display - the backlight should **blink every second**
6. Reconnect USB and check Serial Monitor - if it shows new battery readings, it was running!

## What This Test Does

1. **Enables GPIO15** - Powers the display and battery circuit
2. **Enables GPIO38** - Turns on the backlight
3. **Reads battery voltage** - Shows voltage and percentage
4. **Blinks backlight** - Visual indicator the board is running

## Expected Results

### ✅ Success
- Display backlight blinks every second when on battery
- Battery voltage shows 3.7V - 4.2V
- Board continues running after USB disconnect

### ❌ Failure
- Display goes black when USB disconnected
- No backlight blinking
- Board doesn't respond when USB reconnected

## Troubleshooting

If the test fails:

1. **Check battery connection** - Ensure JST connector is secure
2. **Check battery voltage** - Use multimeter, should be 3.7V - 4.2V
3. **Charge battery** - If voltage is below 3.3V
4. **Try different battery** - Rule out faulty battery
5. **Check board version** - Must be T-Display-S3-AMOLED (not regular T-Display-S3)

## Next Steps

### If Test Succeeds ✅
Your hardware is working! The issue might be in the main W5 sketch. Check:
- Power pins are enabled early enough in setup()
- No code runs before power pins are enabled
- Proper delays after enabling power pins

### If Test Fails ❌
Hardware issue likely. Check:
- Battery health and voltage
- JST connector integrity
- Board for physical damage
- Consider contacting LilyGO support

## Return to Main Sketch

To go back to your main W5 sketch:
1. In Arduino IDE, go to **File → Open**
2. Navigate to: `C:\Users\MashMakesMothershipP\Documents\Arduino\W5\W5\`
3. Open `W5.ino`

## Technical Details

- **GPIO15**: LCD and battery power enable
- **GPIO38**: Backlight enable
- **GPIO4**: Battery voltage ADC input
- **Voltage calculation**: `(ADC * 3.3 * 2.0) / 4095.0`
- **Voltage divider**: 2:1 ratio (100k + 100k resistors)
