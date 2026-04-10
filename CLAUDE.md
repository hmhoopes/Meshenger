# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Meshenger is a decentralized mesh messaging system built on ESP32 microcontrollers. Messages travel over ESP-NOW (a low-level WiFi protocol). A dedicated ESP32 **Pager** acts as a BLE bridge, connecting the mesh to a browser-based web app via Web Bluetooth.

```
Chrome Browser
    ↓ Web Bluetooth (NUS)
ESP32 Pager (BLE bridge)
    ↓ ESP-NOW
ESP32 Mesh Nodes
```

## Build & Flash

**Prerequisite**: Arduino CLI with ESP32 support installed. Run `Setup/arduino-cli.sh` for one-time setup.

```bash
# Build the mesh node firmware
make build

# Build + flash to device
make upload PORT=/dev/ttyUSB0

# Flash + open serial monitor
make flash PORT=/dev/ttyUSB0

# Serial monitor only
make monitor PORT=/dev/ttyUSB0

# Regenerate LSP index (compile_commands.json)
make lsp-index
```

All Arduino sketches use `--fqbn esp32:esp32:esp32:PartitionScheme=huge_app` and baud rate 115200.

**Pager firmware** (built separately from mesh node):
```bash
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=huge_app Pager/ble_serial/ble_serial.ino
arduino-cli upload --fqbn esp32:esp32:esp32:PartitionScheme=huge_app --port <PORT> Pager/ble_serial/ble_serial.ino
```

**Web app** (requires Chrome for Web Bluetooth):
```bash
cd App && python3 -m http.server 8000
```

## Testing

No automated test framework. Tests are done through serial monitor output.

- **Crypto verification**: Flash `Mesh/Crypto/crypto_test/crypto_test.ino` and check serial output for pass/fail on P521 DH and AES operations.
- **Mesh connectivity**: Flash `Mesh/Node/node/node.ino` to multiple ESP32s, monitor serial for Discovery and Text message exchanges.
- **BLE bridge**: Flash Pager, open `App/index.html` in Chrome, connect via Web Bluetooth, verify messages flow through.

## Code Architecture

### Core Mesh Layer (`Mesh/`)

The mesh logic lives in header-only files included by Arduino sketches:

- **`Helpers.hpp`** — Central mesh logic. Owns global state (`Peers` vector, `isPager` flag, `BroadcastPeer`). Key functions: `InitializeESPNow()`, `AnnouceMAC()`, `HandleDiscovery()`, `HandleText()`, `SendTextMessage()` (chunks messages into 243-byte ESP-NOW payloads), `PeersJSON()`.
- **`Message.hpp`** — `Message` struct (type + ID + 243-byte payload), `MessageType` enum, `SendMessage()`, `SendMessageWithRetry()` (blocks up to 5 retries × 500ms per retry).
- **`Peer.hpp`** — Wraps `esp_now_peer_info_t`. Manages peer registration with ESP-NOW.
- **`MAC.hpp`** — MAC address type with hex formatting helpers.
- **`Crypto/Crypto.hpp`** — P521 ECDH + AES-256 encryption. `Tunnel` class represents an encrypted channel. Hardcoded root keys (intentional for this school project — not production-safe).

### BLE Bridge (`Pager/`)

- **`BLE.hpp`** — BLE NUS (Nordic UART Service) server. Device name: `"Meshenger-Pager"`. RX characteristic receives from app; TX characteristic sends to app. Message prefix convention: `'m'` = mesh text (followed by destination MAC + content), `'l'` = peer list JSON, `'r'` = received message.
- **`ble_serial/ble_serial.ino`** — Standalone sketch; bridges BLE serial ↔ hardware serial. The Pager runs this plus mesh includes from `Helpers.hpp` with `isPager = true`.

### Web App (`App/index.html`)

Single self-contained HTML file (no build step, no dependencies). Uses Web Bluetooth API to connect to `"Meshenger-Pager"`. Sends messages as `m<6-byte MAC><text>`. Renders chat bubbles grouped by sender MAC. Dark theme with green accent (`#3fb950`).

### Node Firmware (`Mesh/Node/node/node.ino`)

Minimal sketch: announces presence via Discovery broadcast every few seconds, reads serial input and forwards text to first discovered peer.

## Key Conventions

- **Global state in headers**: `Peers`, `isPager`, etc. are defined (not just declared) in `Helpers.hpp`. Only one translation unit should include `Helpers.hpp`.
- **Broadcast fallback**: If a target peer is not found in the `Peers` list, `SendTextMessage()` broadcasts to all nodes.
- **Message IDs**: Used for deduplication and ACK matching. Retry loop in `SendMessageWithRetry()` matches ACK by sender MAC + message ID.
- **Past messages**: `Mesh/Server/message_store.py` is a Python in-memory store for offline message queuing (not yet integrated into firmware).
