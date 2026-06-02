/*
  Arduino Nano 33 BLE — UART JPEG frame receiver
  
  Wiring:
    ESP32-CAM GPIO14  -->  Arduino Nano 33 BLE RX (pin 0)
    [ESP32-CAM GPIO15  -->  Arduino Nano 33 BLE TX (pin 1)] -- unused
    Common GND

  Frame protocol:
    [0xFF 0xAA]   2 bytes  start marker
    [length]      4 bytes  JPEG payload length, big-endian uint32
    [JPEG data]   N bytes  raw JPEG
    [0xFF 0xBB]   2 bytes  end marker
*/

#include <Arduino.h>

#define CAM_BAUD  115200  // must be the same as camera baud rate

#define START_B1  0xFF
#define START_B2  0xAA
#define END_B1    0xFF
#define END_B2    0xBB

#define MAX_FRAME_SIZE  32768  // 32 KB max
static uint8_t frameBuf[MAX_FRAME_SIZE];

enum RxState {
  WAIT_START_1,   // Looking for 0xFF
  WAIT_START_2,   // Looking for 0xAA
  READ_LEN_0,     // MSB of length
  READ_LEN_1,
  READ_LEN_2,
  READ_LEN_3,     // LSB of length
  READ_PAYLOAD,   // Accumulating JPEG bytes
  WAIT_END_1,     // Looking for 0xFF
  WAIT_END_2      // Looking for 0xBB
};

static RxState   state      = WAIT_START_1;
static uint32_t  frameLen   = 0;
static uint32_t  bytesRead  = 0;

void onFrameReceived(const uint8_t *data, uint32_t len) {
  
  // add preprocessing here

  // Debug
  /*
  Serial.print("Frame OK  len=");
  Serial.print(len);
  Serial.print("  header=0x");
  if (len >= 2) {
    Serial.print(data[0], HEX);
    Serial.print(data[1], HEX);
  }
  Serial.println();
  */

  // forward raw JPEG bytes trough Serial
  Serial.write((uint8_t*)&len, sizeof(len));
  Serial.write(data, len);
}

void setup() {
  Serial.begin(115200);     // Writing image
  Serial1.begin(CAM_BAUD);  // Reading image
}

void loop() {
  while (Serial1.available()) {
    uint8_t b = Serial1.read();

    switch (state) {
      // Waiting for start marker byte 1
      case WAIT_START_1:
        if (b == START_B1) state = WAIT_START_2;
        break;

      // Waiting for start marker byte 2
      case WAIT_START_2:
        if (b == START_B2) {
          state     = READ_LEN_0;
          frameLen  = 0;
          bytesRead = 0;
        } else {
          // False alarm
          state = (b == START_B1) ? WAIT_START_2 : WAIT_START_1;
        }
        break;

      // Read 4-byte big-endian length
      case READ_LEN_0: frameLen  = ((uint32_t)b << 24); state = READ_LEN_1; break;
      case READ_LEN_1: frameLen |= ((uint32_t)b << 16); state = READ_LEN_2; break;
      case READ_LEN_2: frameLen |= ((uint32_t)b <<  8); state = READ_LEN_3; break;
      case READ_LEN_3:
        frameLen |= b;

        if (frameLen == 0 || frameLen > MAX_FRAME_SIZE) {
          // Corrupt length
          Serial.print("Bad frame length: ");
          Serial.println(frameLen);
          state = WAIT_START_1;
        } else {
          state = READ_PAYLOAD;
        }
        break;

      // Accumulate JPEG payload
      case READ_PAYLOAD:
        frameBuf[bytesRead++] = b;
        if (bytesRead >= frameLen) state = WAIT_END_1;
        break;

      // Verify end marker
      case WAIT_END_1:
        if (b == END_B1) {
          state = WAIT_END_2;
        } else {
          Serial.println("Missing end marker byte 1 - discarding frame");
          Serial.print("Expected FF, got ");
          Serial.println(b, HEX);
          state = WAIT_START_1;
        }
        break;

      case WAIT_END_2:
        if (b == END_B2) {
          onFrameReceived(frameBuf, frameLen);
        } else {
          Serial.println("Missing end marker byte 2 - discarding frame");
        }
        state = WAIT_START_1;
        break;

      default:
        state = WAIT_START_1;
        break;
    }
  }
}
