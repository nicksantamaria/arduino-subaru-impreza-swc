#include <SPI.h>
#include <mcp2515.h>

/*
 * Subaru Steering Wheel Control (SWC) CAN Interface
 * Target Hardware: Arduino Nano (USB-C) + MCP2515 Module
 * 
 * Objective: 
 * Read CAN bus messages from Subaru steering wheel buttons and 
 * output corresponding PWM duty cycles to simulate a resistor ladder.
 * 
 * Pinout:
 * MCP2515: CS=10, INT=2
 * PWM Output: D9 (nominated analog-capable pin)
 * SPI: SCK=13, MISO=12, MOSI=11
 */

// --- CONFIGURATION ---
const int CAN_CS_PIN = 10;
const int CAN_INT_PIN = 2;
const int PWM_OUT_PIN = 9;

// CAN Bus Speed
// 2013 Subaru Impreza is typically 500kbps. 
// MCP2515 clock is usually 8MHz or 16MHz. Most modules are 8MHz.
const CAN_SPEED CAN_BAUDRATE = CAN_500KBPS;
const CAN_CLOCK CAN_FREQ = MCP_8MHZ; // Change to MCP_16MHZ if using a 16MHz crystal module

// Likely Steering Wheel Control CAN ID
// Based on research, 0x242 is common for Subaru SWC. 
// Other possibilities: 0x240, 0x241, 0x202.
const uint32_t SWC_CAN_ID = 0x242; 

// PWM Duty Cycle Mappings (0-255)
// These represent the "voltage" the aftermarket head unit will see.
// Adjust these to match what the head unit expects or to ensure distinct separation.
const int VOL_UP_PWM     = 30;
const int VOL_DOWN_PWM   = 60;
const int NEXT_TRACK_PWM = 90;
const int PREV_TRACK_PWM = 120;
const int MODE_PWM       = 150;
const int MUTE_PWM       = 180;
const int CALL_ANS_PWM   = 210;
const int CALL_REJ_PWM   = 240;
const int NO_BUTTON_PWM  = 0;   // 0V when no button is pressed

// --- GLOBALS ---
MCP2515 mcp2515(CAN_CS_PIN);
struct can_frame canMsg;

void setup() {
  Serial.begin(115200);
  while (!Serial);

  pinMode(PWM_OUT_PIN, OUTPUT);
  analogWrite(PWM_OUT_PIN, NO_BUTTON_PWM);

  mcp2515.reset();
  if (mcp2515.setBitrate(CAN_BAUDRATE, CAN_FREQ) != MCP2515::ERROR_OK) {
    Serial.println("Error setting MCP2515 Bitrate!");
  }
  mcp2515.setNormalMode();

  Serial.println("Subaru SWC Interface Started");
  Serial.print("Listening for CAN ID: 0x");
  Serial.println(SWC_CAN_ID, HEX);
}

void loop() {
  if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
    
    // Always print raw data for debugging and identification
    printCanFrame(&canMsg);

    // Process the message if it matches our target ID
    if (canMsg.can_id == SWC_CAN_ID) {
      handleSWC(canMsg.data, canMsg.can_dlc);
    }
  }
}

/**
 * Maps the CAN data byte to a PWM output.
 * Note: The exact byte and bit mapping needs to be confirmed.
 * Common Subaru mapping: Byte 0 or Byte 1 contains a bitmask or value.
 */
void handleSWC(uint8_t* data, uint8_t len) {
  if (len < 1) return;

  int targetPWM = NO_BUTTON_PWM;

  // Placeholder logic: Assuming Byte 0 holds the button value.
  // FLAG: This logic must be verified against actual vehicle data.
  uint8_t buttonCode = data[0]; 

  switch (buttonCode) {
    case 0x01: targetPWM = VOL_UP_PWM;     Serial.println(">> Button: VOL UP"); break;
    case 0x02: targetPWM = VOL_DOWN_PWM;   Serial.println(">> Button: VOL DOWN"); break;
    case 0x03: targetPWM = NEXT_TRACK_PWM; Serial.println(">> Button: NEXT"); break;
    case 0x04: targetPWM = PREV_TRACK_PWM; Serial.println(">> Button: PREV"); break;
    case 0x05: targetPWM = MODE_PWM;       Serial.println(">> Button: MODE"); break;
    case 0x06: targetPWM = MUTE_PWM;       Serial.println(">> Button: MUTE"); break;
    case 0x07: targetPWM = CALL_ANS_PWM;   Serial.println(">> Button: CALL ANSWER"); break;
    case 0x08: targetPWM = CALL_REJ_PWM;   Serial.println(">> Button: CALL REJECT"); break;
    case 0x00: targetPWM = NO_BUTTON_PWM;  break;
    default:
      // Unknown button or multiple buttons pressed
      targetPWM = NO_BUTTON_PWM;
      break;
  }

  analogWrite(PWM_OUT_PIN, targetPWM);
}

void printCanFrame(struct can_frame* frame) {
  Serial.print("ID: 0x");
  Serial.print(frame->can_id, HEX);
  Serial.print(" [");
  Serial.print(frame->can_dlc);
  Serial.print("] ");
  
  for (int i = 0; i < frame->can_dlc; i++) {
    if (frame->data[i] < 0x10) Serial.print("0");
    Serial.print(frame->data[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
}
