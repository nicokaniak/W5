# W5 Menu System

## Overview
The W5 smartwatch now includes a menu system that allows you to navigate between different features using the physical buttons on the device.

## Button Controls
- **Button 1 (GPIO 0)**: Press to cycle through screens
- **Button 2 (GPIO 21)**: Reserved for future use (select/back functionality)

## Available Screens

### 1. Watch Face (Default)
- Displays current time in large cyan text
- Shows battery indicator bar at bottom-right
- This is the default screen on startup

### 2. Weather Screen
- Title in yellow
- Displays temperature, conditions, and location
- Note: Requires WiFi connection for data

### 3. Alarms Screen
- Title in magenta
- Shows alarm time and status
- Displays whether alarms are active/inactive

### 4. Battery Screen
- Title in green
- Shows detailed battery information:
  - Voltage (in volts)
  - Charge percentage
  - Large visual battery bar with color coding:
    - Green: >50%
    - Yellow: 20-50%
    - Red: <20%
  - GPIO15 power status

### 5. Bluetooth Screen
- Title in blue
- Shows Bluetooth connection status
- Displays notifications (when available)
- Instructions for pairing with phone

## Navigation Flow
Watch Face → Weather → Alarms → Battery → Bluetooth → (back to Watch Face)

## Technical Details

### Files Added
- `MenuManager.h` - Menu system interface
- `MenuManager.cpp` - Button handling and navigation logic

### Files Modified
- `DisplayManager.h` - Added new screen drawing methods
- `DisplayManager.cpp` - Implemented screen layouts
- `W5.ino` - Integrated menu system into main loop

### Features
- **Debounced button input**: 200ms debounce delay prevents accidental double-presses
- **Non-blocking**: Background tasks (time updates, alarms, battery monitoring) continue running on all screens
- **Responsive**: 100ms update cycle for smooth button response
- **Serial logging**: Button presses and screen changes are logged to Serial monitor

## Usage
1. Upload the code to your LilyGO T-Display-S3-AMOLED
2. Press Button 1 (GPIO 0) to navigate between screens
3. Watch the Serial monitor to see button press events and screen transitions

## Future Enhancements
- Button 2 can be used for selecting items or going back
- Weather screen can be enhanced to show live data from WeatherManager
- Alarms screen can show actual alarm times from AlarmManager
- Bluetooth screen can display real notifications from BluetoothManager
