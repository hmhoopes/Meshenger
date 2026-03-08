# Meshenger Web App

Browser-based messaging UI for **Meshenger**.  
It connects to a nearby ESP32 “LocalESP” device over **Bluetooth Low Energy (BLE)** and provides:

- A **Peers** view for managing connections.
- A **Messages** view (chat window) for the selected peer.
- A **Settings** view with simple toggles (notification sounds, brightness).

The web app is a single HTML page (`index.html`) using plain **HTML/CSS/JavaScript** and the **Web Bluetooth API** (no frontend framework).

---

## What the app does

- **Discovers and connects** to an ESP32 running the Nordic UART Service (NUS) (“Meshenger‑LocalESP”).
- **Sends text messages** from the browser to the ESP32 over BLE.
- **Receives messages** from the ESP32 and displays them as chat bubbles.
- Organizes the UI into three main sections:
  - **Peers**: list of known/available peers you can start a chat with.
  - **Messages**: dedicated chat window for the currently selected peer.
  - **Settings**: toggles for notification sounds and a high-brightness display mode (wired for future behavior).

Today, messages go between the browser and LocalESP; the LocalESP → mesh forwarding is handled in the firmware.

---

## Requirements

- **Chrome** (or another browser with [Web Bluetooth](https://caniuse.com/web-bluetooth) support).
- **HTTPS or localhost** — Web Bluetooth only works in secure contexts.
- An **ESP32 LocalESP** device running the BLE NUS sketch  
  (e.g. `Pager/ble_serial/ble_serial.ino` or the equivalent in this repo).

---

## How to run the app

From the repo root:

1. **Recommended: simple local server**

   ```bash
   cd App
   python3 -m http.server 8000
   ```

   Then open **http://localhost:8000** in Chrome.

2. **Any static server**

   Serve the `App` directory with any static file server (must be HTTPS or `localhost`), for example:

   ```bash
   npx serve App -p 3000
   ```

   Then open `http://localhost:3000` or the URL it prints.

3. **Direct file open (not recommended)**

   Opening `index.html` as `file://` may not work in all browsers because Web Bluetooth is blocked in that context. Prefer a local server.

---

## How to use it

1. Flash your **LocalESP** ESP32 with the BLE NUS sketch.
2. Power the ESP32 and wait for it to advertise (e.g. as **"Meshenger-LocalESP"**).
3. Open the web app in Chrome (`http://localhost:8000` or your chosen URL).
4. Click **Connect to device**, select your ESP32 in the Web Bluetooth device picker, and connect.
5. Use the **Peers** view to select a peer and open a dedicated chat window.
6. In the **Messages** view, type a message and press Enter or **Send**.  
   The message is sent over BLE to the ESP32; any replies from the device appear as received bubbles.
7. In **Settings**, toggle notification sounds and high-brightness display (hooks for future UX behavior).

---

## Architecture

High-level data path:

```
Browser (Web Bluetooth UI)
      ↓           ↑
  Nordic UART Service (NUS) over BLE
      ↓           ↑
 LocalESP (ESP32 firmware)
      ↓           ↑
  Serial / mesh routing (future / handled in ESP32 code)
```

Key points:

- The app uses **standard NUS UUIDs**, so it works with any compatible BLE UART firmware.
- All UI logic lives in `App/index.html`:
  - DOM structure for the three sections (Peers / Messages / Settings).
  - Web Bluetooth connection and NUS read/write logic.
  - Simple in-memory state for selected peer and current section.

