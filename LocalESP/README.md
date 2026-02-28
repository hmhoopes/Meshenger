# LocalESP

ESP32 device paired with the Meshenger app. Two options:

## 1. BLE (for web app) — `ble_serial/`

- **Sketch:** `ble_serial/ble_serial.ino`
- Exposes **Nordic UART Service (NUS)** over BLE.
- Use with the **web app** in `App/`: connect in Chrome via Web Bluetooth to **"Meshenger-LocalESP"**.
- Data received from the app is printed on Serial; data from Serial is sent to the app (e.g. for future mesh→app messages).

## 2. Classic Bluetooth (SPP) — `ping_test.cpp`

- **Sketch:** `ping_test.cpp` (rename/copy to `.ino` or use in a build that supports it)
- Serial bridge over **Classic Bluetooth SPP**. Pairs with phones/PCs as a serial device; not usable from the browser Web Bluetooth API.

For the **web application** interface, use **BLE** and the `ble_serial` sketch.
