# Quick Start: Fixing Battery Power Issue

## Your Problem
Board works with USB+Battery, but shuts down when you disconnect USB (even with battery connected).

## What I've Done

### 1. Updated Main Sketch (W5.ino)
Added **GPIO hold functionality** to maintain power during USB disconnect:
- `gpio_hold_en((gpio_num_t)15)` - Keeps GPIO15 HIGH during power transitions
- `gpio_hold_en((gpio_num_t)38)` - Keeps backlight on

**This may fix your issue!** The GPIO hold feature maintains the pin state even during brief power glitches.

### 2. Created Test Sketches

#### a) hardware_diagnostic.ino
**Purpose:** Identify if the problem is hardware or software

**How to use:**
1. Open `battery_test/hardware_diagnostic.ino` in Arduino IDE
2. Upload to board
3. Open Serial Monitor (115200 baud)
4. Read the diagnostic results
5. Follow the specific recommendations

**What it checks:**
- Battery voltage and connection quality
- GPIO15 and GPIO38 states
- ADC reading stability
- Provides pass/fail results for each test

#### b) battery_test_enhanced.ino
**Purpose:** Test battery power with advanced power hold techniques

**How to use:**
1. Open `battery_test/battery_test_enhanced.ino` in Arduino IDE
2. Upload to board
3. Open Serial Monitor (115200 baud)
4. Note the "Boot count" number
5. Disconnect USB cable
6. Wait 10 seconds (watch for backlight blinking)
7. Reconnect USB
8. Check if boot count increased

**If boot count increased:** ✅ Board stayed on! Hardware works.
**If boot count same:** ❌ Board shut down. Hardware issue.

### 3. Created Documentation

#### BATTERY_POWER_ISSUE.md
Comprehensive troubleshooting guide with:
- Detailed explanation of why this happens
- 5 different solutions to try
- Hardware modification instructions (if needed)
- Testing procedures
- Common issues and fixes

## What to Do Now

### Option 1: Try Updated Main Sketch (Easiest)
1. Upload the updated `W5.ino` to your board
2. Open Serial Monitor
3. Disconnect USB cable
4. Wait 10 seconds
5. Reconnect USB
6. Check if it kept running (Serial messages should continue from where they left off)

### Option 2: Run Diagnostics First (Recommended)
1. Upload `hardware_diagnostic.ino`
2. Follow the on-screen instructions
3. This will tell you if the problem is hardware or software
4. Then proceed based on results

### Option 3: Test with Enhanced Sketch
1. Upload `battery_test_enhanced.ino`
2. Test if board stays on when USB disconnected
3. If yes, the main W5 sketch should also work now
4. If no, you may need hardware modification

## Understanding the Problem

**The Issue:**
- LilyGO T-Display-S3-AMOLED requires GPIO15 HIGH to enable battery power
- When you disconnect USB, there's a brief power glitch
- GPIO15 goes LOW during this glitch
- Power management IC sees LOW signal and cuts battery power
- Board shuts down

**The Solution:**
- `gpio_hold_en()` maintains GPIO15 HIGH even during power glitches
- This keeps the power management IC enabled
- Board stays on when USB is disconnected

## If Software Fix Doesn't Work

You may need a **hardware pull-up resistor** on GPIO15:

**What you need:**
- 10kΩ resistor
- Soldering iron

**What to do:**
- Solder resistor between GPIO15 and 3.3V
- This physically holds GPIO15 HIGH during transitions
- See BATTERY_POWER_ISSUE.md for detailed instructions

## Files Created/Modified

### Modified:
- `W5/W5.ino` - Added GPIO hold functionality

### Created:
- `battery_test/hardware_diagnostic.ino` - Hardware testing
- `battery_test/battery_test_enhanced.ino` - Enhanced battery test
- `BATTERY_POWER_ISSUE.md` - Comprehensive troubleshooting guide
- `QUICK_START.md` - This file

## Expected Results

After applying the fix:
- ✅ Board runs on battery when USB disconnected
- ✅ Display stays on
- ✅ Battery voltage reading is accurate
- ✅ Can switch between USB and battery seamlessly

## Need More Help?

1. Read `BATTERY_POWER_ISSUE.md` for detailed troubleshooting
2. Run `hardware_diagnostic.ino` to identify specific issues
3. Check LilyGO GitHub: https://github.com/Xinyuan-LilyGO/T-Display-S3-AMOLED/issues
4. Contact LilyGO support if hardware defect suspected

## Quick Reference

**Battery Voltage Readings:**
- 4.2V = Fully charged
- 3.7V = Normal
- 3.3V = Low (charge soon)
- <3.0V = Too low (won't boot)

**Power Pins:**
- GPIO15 = Power enable (MUST be HIGH)
- GPIO38 = Backlight enable
- GPIO4 = Battery voltage ADC

**Test Procedure:**
1. Upload sketch
2. Open Serial Monitor (115200 baud)
3. Disconnect USB
4. Wait 10 seconds
5. Reconnect USB
6. Check if board kept running

Good luck! Start with Option 1 (try the updated W5.ino) - it may just work! 🚀
