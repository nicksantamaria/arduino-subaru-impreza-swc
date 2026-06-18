# Subaru SWC CAN Interface

This project provides Arduino Nano firmware to interface between a 2013 Subaru Impreza (GJ/GP) CAN bus and an aftermarket head unit's resistive Steering Wheel Control (SWC) input.

## Goal

The objective is to read steering wheel button presses from the car's CAN bus (via the OBD2 port) and translate them into specific analog voltage levels. These levels are generated using PWM on the Arduino and are designed to be read by the head unit's SWC "KEY" wire, which typically uses a resistor-ladder protocol.

### Supported Functions
- Volume Up / Down
- Next / Previous Track
- Mode
- Mute
- Call Answer / Reject

## Hardware Requirements

- **Microcontroller**: Arduino Nano (USB-C version recommended)
- **CAN Interface**: MCP2515 CAN Bus Module
- **Connection**: OBD2 Port (CAN High: Pin 6, CAN Low: Pin 14)
- **Output**: PWM Pin (D9) connected to the head unit's SWC wire.

## Software Setup

This project uses `arduino-cli` for compilation and deployment. A `Makefile` is provided to simplify common tasks.

### 1. Install Dependencies
Ensure you have `arduino-cli` installed on your system. Then, run the following command to install the required AVR core and the MCP2515 library:

```bash
make install-deps
```

### 2. Compile
To compile the firmware:

```bash
make
```

### 3. Upload
Connect your Arduino Nano and run (adjust the `PORT` in the `Makefile` if necessary):

```bash
make upload
```

### 4. Monitor
To view serial debug output (raw CAN IDs and button detection):

```bash
make monitor
```

## Configuration

You can tune the following constants at the top of `arduino-subaru-impreza-swc.ino`:
- `CAN_FREQ`: Set to `MCP_8MHZ` or `MCP_16MHZ` depending on your module's crystal.
- `SWC_CAN_ID`: The CAN ID for steering wheel messages (defaulted to `0x242`).
- `PWM_OUT_PIN`: The Arduino pin used for PWM output (defaulted to `D9`).
- `VOL_UP_PWM`, etc.: The duty cycle values (0-255). These are pre-configured to match the **Sony resistive ladder standard** (similar to RM-X4S), making it compatible with most aftermarket head units that support Sony remotes or have a learning mode.

## TODO: Physical Build Specs

- [ ] **Wiring Diagram**: Create a schematic showing connections between Nano, MCP2515, and OBD2 port.
- [ ] **RC Filter**: Design and document a simple RC low-pass filter (e.g., 10kΩ resistor + 10µF capacitor) to smooth the PWM signal into a steady DC voltage for the head unit.
- [ ] **Enclosure**: Design or select a suitable 3D-printable case for the electronics.
- [ ] **Vehicle Verification**: Confirm exact CAN IDs and data bytes for the 2013 Subaru Impreza via the Serial Monitor during bench testing.
