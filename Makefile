# Author: Aniketh Aatipamula
# Inputs: Commands to execute
# Outputs: Builds for ESP32 Boards
# Date Created: Feb 25, 2026

# NOTE: Variables created with '?=' can be set from the command line
#       e.g. 'make flash PORT=/dev/ttyUSB1 BUILD_DIR=./Pager'

PARTITION ?= huge_app
FQBN      =  esp32:esp32:esp32:PartitionScheme=$(PARTITION)
PORT      ?= /dev/ttyUSB0
BAUD      ?= 115200

BUILD_DIR ?= ./Mesh/Nodes/node

.PHONY: build upload monitor lsp-index tui

build:
	arduino-cli compile --fqbn $(FQBN) \
		--build-path $(BUILD_DIR)/build \
		$(BUILD_DIR)

	# Simlink compile_commands to top level dir
	ln -sf $(BUILD_DIR)/build/compile_commands.json .

upload: build
	arduino-cli upload --fqbn $(FQBN) --port $(PORT) \
		--input-dir $(BUILD_DIR)/build \
		$(BUILD_DIR)

monitor:
	arduino-cli monitor --port $(PORT) --config baudrate=$(BAUD)

# Monitor using the python tui for better interface (posix only)
tui:
	python3 ./monitor.py $(PORT) $(BAUD)

flash: upload tui

# Regenerate LSP compile_commands.json
lsp-index:
	arduino-cli compile --fqbn $(FQBN) \
		--build-path $(BUILD_DIR)/build --only-compilation-database \
		$(BUILD_DIR)

	# Simlink compile_commands to top level dir
	ln -sf $(BUILD_DIR)/build/compile_commands.json .
