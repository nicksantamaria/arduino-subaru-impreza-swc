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
// Arduino Nano pin connected to MCP2515 CS (Chip Select)
const int CAN_CS_PIN = 10;
// Arduino Nano pin connected to MCP2515 INT (Interrupt)
const int CAN_INT_PIN = 2;
// Arduino Nano pin for PWM output to Head Unit SWC wire
const int PWM_OUT_PIN = 9;
// Arduino Nano pin for LED indicator
const int LED_PIN = 8;

// Latency reduction: Compile-time debug logging
// Set to 1 to enable Serial logging, 0 to disable for maximum performance
#define DEBUG_CAN 0

#if DEBUG_CAN
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
  #define DEBUG_PRINT_HEX(x, y) Serial.print(x, y)
  #define DEBUG_LOG_FRAME(x) printCanFrame(x)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINT_HEX(x, y)
  #define DEBUG_LOG_FRAME(x)
#endif

// CAN Bus Speed
// 2013 Subaru Impreza is typically 500kbps. 
// MCP2515 clock is usually 8MHz or 16MHz. Most modules are 8MHz.
// Speed of the vehicle's CAN bus (standard for most Subarus)
const CAN_SPEED CAN_BAUDRATE = CAN_500KBPS;
// Frequency of the crystal on your MCP2515 module
const CAN_CLOCK CAN_FREQ = MCP_8MHZ; // Change to MCP_16MHZ if using a 16MHz crystal module

// Likely Steering Wheel Control CAN ID
// Based on research, 0x242 is common for Subaru SWC. 
// Other possibilities: 0x240, 0x241, 0x202.
// The specific CAN ID that carries steering wheel button data
const uint32_t SWC_CAN_ID = 0x242; 

// PWM Duty Cycle Mappings (0-255)
// These represent the "voltage" levels for a Sony-compatible resistor ladder.
// Standard Sony RM-X4S resistance values (approximate):
// OFF: High Impedance, SOURCE: 2.2k, MUTE: 4.4k, VOL+: 16.8k, VOL-: 23.6k, SEEK+: 8.8k, SEEK-: 12.1k
// PWM values below are calibrated for a 5V system with the head unit's internal pull-up.
// Since the head unit has a learning mode, these are distinct steps following the Sony standard.
// PWM level for Volume Up button
const int VOL_UP_PWM     = 140; // ~2.7V
// PWM level for Volume Down button
const int VOL_DOWN_PWM   = 175; // ~3.4V
// PWM level for Next Track / Seek Up button
const int NEXT_TRACK_PWM = 90;  // ~1.8V
// PWM level for Previous Track / Seek Down button
const int PREV_TRACK_PWM = 115; // ~2.2V
// PWM level for Mode / Source button
const int MODE_PWM       = 45;  // ~0.9V (Source)
// PWM level for Mute / Attenuate button
const int MUTE_PWM       = 65;  // ~1.3V (Attenuate)
// PWM level for Answer Call button
const int CALL_ANS_PWM   = 25;  // ~0.5V (Phone/Answer)
// PWM level for Reject/Hang-up button
const int CALL_REJ_PWM   = 10;  // ~0.2V (Reject)
// PWM level for Voice Activation button (Listening for commands)
const int VOICE_ACT_PWM  = 200; // ~3.9V (Custom/Voice)
// PWM level when no button is pressed (idle state)
const int NO_BUTTON_PWM  = 0;   // 0V (High resistance/Open circuit)

// --- GLOBALS ---
MCP2515 mcp2515(CAN_CS_PIN);
struct can_frame canMsg;
volatile bool interruptFlag = false;
int lastPWMValue = -1; // Track last PWM to avoid redundant analogWrite calls

void irqHandler() {
  interruptFlag = true;
}

void setup() {
  Serial.begin(115200);
  while (!Serial);

  pinMode(PWM_OUT_PIN, OUTPUT);
  analogWrite(PWM_OUT_PIN, NO_BUTTON_PWM);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  mcp2515.reset();
  if (mcp2515.setBitrate(CAN_BAUDRATE, CAN_FREQ) != MCP2515::ERROR_OK) {
    Serial.println("Error setting MCP2515 Bitrate!");
  }

  // Hardware Filtering: Only allow SWC_CAN_ID (0x242)
  // Mask 0 (for RXB0): Check all 11 bits of standard ID
  mcp2515.setFilterMask(MCP2515::MASK0, false, 0x7FF);
  mcp2515.setFilter(MCP2515::RXF0, false, SWC_CAN_ID);
  mcp2515.setFilter(MCP2515::RXF1, false, SWC_CAN_ID);

  // Mask 1 (for RXB1): Check all 11 bits of standard ID
  mcp2515.setFilterMask(MCP2515::MASK1, false, 0x7FF);
  mcp2515.setFilter(MCP2515::RXF2, false, SWC_CAN_ID);
  mcp2515.setFilter(MCP2515::RXF3, false, SWC_CAN_ID);
  mcp2515.setFilter(MCP2515::RXF4, false, SWC_CAN_ID);
  mcp2515.setFilter(MCP2515::RXF5, false, SWC_CAN_ID);

  mcp2515.setNormalMode();

  pinMode(CAN_INT_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(CAN_INT_PIN), irqHandler, FALLING);

  Serial.println("Subaru SWC Interface Started");
  Serial.print("Listening for CAN ID: 0x");
  Serial.println(SWC_CAN_ID, HEX);
}

void loop() {
  bool pending = false;
  
  // Atomic read and clear of the interrupt flag
  noInterrupts();
  if (interruptFlag) {
    interruptFlag = false;
    pending = true;
  }
  interrupts();

  // Process messages if flag was set OR if INT pin is LOW (back-to-back messages)
  if (pending || digitalRead(CAN_INT_PIN) == LOW) {
    // Drain all available frames from MCP2515 buffers
    while (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
      // Latency: Guarded debug prints (Zero cost when DEBUG_CAN is 0)
      DEBUG_LOG_FRAME(&canMsg);

      // Safe ID comparison: Mask out EFF/RTR/ERR bits (0x7FF for standard IDs)
      if ((canMsg.can_id & 0x7FF) == SWC_CAN_ID) {
        handleSWC(canMsg.data, canMsg.can_dlc);
      }
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
    case 0x01: targetPWM = VOL_UP_PWM;     DEBUG_PRINTLN(">> Button: VOL UP"); break;
    case 0x02: targetPWM = VOL_DOWN_PWM;   DEBUG_PRINTLN(">> Button: VOL DOWN"); break;
    case 0x03: targetPWM = NEXT_TRACK_PWM; DEBUG_PRINTLN(">> Button: NEXT"); break;
    case 0x04: targetPWM = PREV_TRACK_PWM; DEBUG_PRINTLN(">> Button: PREV"); break;
    case 0x05: targetPWM = MODE_PWM;       DEBUG_PRINTLN(">> Button: MODE"); break;
    case 0x06: targetPWM = MUTE_PWM;       DEBUG_PRINTLN(">> Button: MUTE"); break;
    case 0x07: targetPWM = CALL_ANS_PWM;   DEBUG_PRINTLN(">> Button: CALL ANSWER"); break;
    case 0x08: targetPWM = CALL_REJ_PWM;   DEBUG_PRINTLN(">> Button: CALL REJECT"); break;
    case 0x09: targetPWM = VOICE_ACT_PWM;  DEBUG_PRINTLN(">> Button: VOICE ACT"); break;
    case 0x00: targetPWM = NO_BUTTON_PWM;  break;
    default:
      // Unknown button or multiple buttons pressed
      targetPWM = NO_BUTTON_PWM;
      break;
  }

  // Optimization: Only write if value changed
  if (targetPWM != lastPWMValue) {
    analogWrite(PWM_OUT_PIN, targetPWM);
    
    // Light up LED when a button is pressed
    if (targetPWM != NO_BUTTON_PWM) {
      digitalWrite(LED_PIN, HIGH);
    } else {
      digitalWrite(LED_PIN, LOW);
    }

    lastPWMValue = targetPWM;
  }
}

void printCanFrame(struct can_frame* frame) {
  Serial.print("ID: 0x");
  Serial.print(frame->can_id & 0x7FF, HEX);
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
