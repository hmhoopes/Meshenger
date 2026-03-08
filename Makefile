# Makefile
# PartitionScheme=huge_app gives ~3MB app partition so BLE+WiFi+mesh sketch fits (default is ~1.3MB)
FQBN     = esp32:esp32:esp32:PartitionScheme=huge_app
PORT     ?= /dev/ttyUSB0
BAUD     ?= 115200
NODE_DIR = Mesh/Node/node
ESP_DIR  = Pager

.PHONY: build upload monitor lsp-index

build:
	arduino-cli compile --fqbn $(FQBN) $(NODE_DIR)

upload: build
	arduino-cli upload --fqbn $(FQBN) --port $(PORT) $(NODE_DIR)

monitor:
	arduino-cli monitor --port $(PORT) --config baudrate=$(BAUD)

flash: upload monitor

# Regenerate LSP compile_commands.json
lsp-index:
	bear -- arduino-cli compile --fqbn $(FQBN) $(NODE_DIR)
