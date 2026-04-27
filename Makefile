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

BUILD_DIR  ?= ./Mesh/Nodes/node
PAGER_DIR  ?= ./Mesh/Nodes/pager

# OTA requires two app partitions; min_spiffs gives 1.8 MB each.
# If the binary exceeds 1.8 MB, create a custom partition table instead.
PAGER_PARTITION ?= min_spiffs
PAGER_FQBN      = esp32:esp32:esp32:PartitionScheme=$(PAGER_PARTITION)

.PHONY: build upload monitor lsp-index tui build-pager upload-pager flash-pager

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

# Pager targets (use min_spiffs partition to enable OTA)
build-pager:
	arduino-cli compile --fqbn $(PAGER_FQBN) \
		--build-path $(PAGER_DIR)/build \
		$(PAGER_DIR)

upload-pager: build-pager
	arduino-cli upload --fqbn $(PAGER_FQBN) --port $(PORT) \
		--input-dir $(PAGER_DIR)/build \
		$(PAGER_DIR)

flash-pager: upload-pager
	arduino-cli monitor --port $(PORT) --config baudrate=$(BAUD)

# Regenerate LSP compile_commands.json
lsp-index:
	arduino-cli compile --fqbn $(FQBN) \
		--build-path $(BUILD_DIR)/build --only-compilation-database \
		$(BUILD_DIR)

	# Simlink compile_commands to top level dir
	ln -sf $(BUILD_DIR)/build/compile_commands.json .
