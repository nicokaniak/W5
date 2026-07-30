# Bluetooth Low Energy (BLE) Guide

## Overview
The W5 smartwatch uses **Bluetooth Low Energy (BLE)** because the ESP32-S3 chip does not support Classic Bluetooth.

## How to Connect
Unlike Classic Bluetooth, you **do not pair** in your phone's system settings. Instead, you connect directly through a compatible app.

### Android
1.  Download **"Serial Bluetooth Terminal"** by Kai Morich.
2.  Open the app and go to the **Devices** menu (hamburger menu -> Devices).
3.  Switch to the **"Bluetooth LE"** tab.
4.  Scan for devices.
5.  Select **"Lilygo_Watch"** to connect.

### iOS
1.  Download **"Bluefruit Connect"** by Adafruit or **"nRF Connect"**.
2.  Open the app and scan for peripherals.
3.  Find **"Lilygo_Watch"** and tap **Connect**.
4.  Select **UART** mode to send text messages.

## Sending Notifications
Once connected via the app:
1.  Type a message in the terminal/UART interface.
2.  Press Send.
3.  The message will appear on the watch's **Bluetooth Screen**.

## Troubleshooting
- If the device doesn't appear, try restarting Bluetooth on your phone.
- Ensure no other device is currently connected to the watch.
- The watch advertises as "Lilygo_Watch".

## Technical Details
- **Service UUID**: `6E400001-B5A3-F393-E0A9-E50E24DCCA9E` (Nordic UART Service)
- **RX Characteristic**: `6E400002...` (Write)
- **TX Characteristic**: `6E400003...` (Notify)
