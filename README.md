# Meshenger

Mesh based messaging system using ESP Now.

# Development

## Arduino CLI

1. Install [arduino-cli](https://docs.arduino.cc/arduino-cli/)
2. Run the setup script (Linux/Mac)

```bash
./Setup/arduino-cli.sh init
```

3. Use the following commands to build/flash onto ESP32

```bash
arduino-cli board list   # This will list available boards for you to flash

make build               # Build the project
make upload              # This will upload the compiled code
make monitor             # This will open the serial monitor
make flash               # This will upload and open the serial monitor

make flash PORT=/dev/ttyUSB1  # Will change the device to USB1 and flash.
                              #   Refer to the output of `arduino-cli board list` for available ports
```
