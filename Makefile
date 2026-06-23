# Makefile for Subaru SWC Arduino Project
# Uses arduino-cli: https://arduino.github.io/arduino-cli/

# Configuration
BOARD = arduino:avr:nano
# For newer Nano with ATmega328P (Old Bootloader), use:
#BOARD = arduino:avr:nano:cpu=atmega328pold
PORT = /dev/cu.usbserial-110
SKETCH = arduino-subaru-impreza-swc.ino
BUILD_PATH = build

# Libraries
LIB_MCP2515 = "autowp-mcp2515"

.PHONY: all compile upload install-deps clean check-cli

all: compile

# Check if arduino-cli is installed
check-cli:
	@arduino-cli version > /dev/null 2>&1 || (echo "Error: arduino-cli not found. Please install it first." && exit 1)

# Install required libraries and core
install-deps: check-cli
	arduino-cli core update-index
	arduino-cli core install arduino:avr
	arduino-cli lib install $(LIB_MCP2515)

# Compile the sketch
compile: check-cli
	@arduino-cli core list | grep -q "arduino:avr" || (echo "Error: Platform 'arduino:avr' not found. Run 'make install-deps' first." && exit 1)
	arduino-cli compile --fqbn $(BOARD) --build-path $(BUILD_PATH) $(SKETCH)

# Upload the sketch to the board
upload: check-cli
	arduino-cli upload -p $(PORT) --fqbn $(BOARD) --build-path $(BUILD_PATH) $(SKETCH)

# Clean build artifacts
clean:
	rm -rf $(BUILD_PATH)

# Monitor serial output
monitor: check-cli
	arduino-cli monitor -p $(PORT) -c baudrate=115200
