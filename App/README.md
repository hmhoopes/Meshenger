# Meshenger App

Web messaging interface for the LocalESP device. Connects to the ESP32 over **Bluetooth Low Energy (BLE)** using the Nordic UART Service (NUS).

## Requirements

- **Chrome** (or another browser with [Web Bluetooth](https://caniuse.com/web-bluetooth) support)
- **HTTPS or localhost** — Web Bluetooth only works in secure contexts
- **LocalESP** running the BLE NUS sketch (`LocalESP/ble_serial/`)

## Run the web app

1. **Option A: Local server (recommended)**  
   From the repo root:
   ```bash
   cd App && python3 -m http.server 8000
   ```
   Open **http://localhost:8000** in Chrome.

2. **Option B: Any static server**  
   Serve the `App` folder over HTTPS or from `localhost` (e.g. `npx serve App -p 3000`), then open the URL in Chrome.

3. **Option C: Open file**  
   Opening `index.html` as `file://` may not work in all browsers due to Web Bluetooth security rules. Prefer localhost.

## Usage

1. Flash **LocalESP** with `LocalESP/ble_serial/ble_serial.ino` (Arduino IDE, ESP32 board).
2. Power the ESP32 and wait until it advertises as **"Meshenger-LocalESP"**.
3. Open the web app in Chrome (via localhost or HTTPS).
4. Click **Connect to device**, choose **Meshenger-LocalESP** in the browser’s device picker.
5. Type messages and press Enter or **Send**. Messages go to the ESP32 over BLE; replies from the device appear in the chat.

## Architecture

```
Browser (Web Bluetooth)  ←→  BLE NUS  ←→  LocalESP (ESP32)
                                    ←→  Serial / mesh (future)
```

The app uses the standard Nordic UART Service UUIDs so it can work with any NUS-compatible BLE device.
