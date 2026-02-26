# Makefile
FQBN     = esp32:esp32:esp32
PORT     ?= /dev/ttyUSB0
BAUD     ?= 115200
NODE_DIR = Mesh/Node/node
ESP_DIR  = LocalESP

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
