#/bin/bash

set -e # Exit if any command fails

init() {
  # Initialize config
  arduino-cli config init

  # Add ESP32 board manager URL
  arduino-cli config add board_manager.additional_urls \
    https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json

  # Update index and install ESP32 platform
  arduino-cli core update-index
  arduino-cli core install esp32:esp32

  # Verify ESP32 boards are available
  arduino-cli board listall esp32
}

if [[ "$1" = "init" ]]; then
  init
else
  echo "Invalid option" >& 2
fi
