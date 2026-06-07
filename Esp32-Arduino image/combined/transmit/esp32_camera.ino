/*
  ESP32-CAM: MJPEG WiFi stream + UART image transmitter

  Wiring:
    ESP32-CAM GPIO14  -->  Arduino Nano 33 BLE RX (pin 0)
    [ESP32-CAM GPIO15  -->  Arduino Nano 33 BLE TX (pin 1)]
    Common GND

  Frame protocol:
    [0xFF 0xAA]   2 bytes  start marker
    [length]      4 bytes  JPEG payload length, big-endian uint32
    [JPEG data]   N bytes  raw JPEG
    [0xFF 0xBB]   2 bytes  end marker
*/

#include <Arduino.h>

#include "config.h"
#include "wifi_handle.h"
#include "frame_handle.h"

void setup() {
  // unstable above 80
  setCpuFrequencyMhz(80);

  Serial.begin(115200);
  Serial.println("ESP32-CAM: booting");

  Serial2.begin(UART_BAUD, SERIAL_8N1, CAM_UART_RX, CAM_UART_TX);

  if (!initCamera()) {
    Serial.println("Camera: failed");
    while (true) delay(1000);
  }
  Serial.println("Camera OK");
  
  if (ENABLE_WIFI) initWifi();
  else Serial.println("Wifi disabled");

  Serial.print("Capture mode: ");
  Serial.println(CAMERA_MODE==RGB565?"RGB565":"JPEG");
  Serial.println("Sending frames on GPIO14");
}

void loop() {
  if (Serial2.available()) {
    uint8_t b = Serial2.read();
    if (b == REQ_BYTE) handleFrameRequest();
  }

  if (ENABLE_WIFI) handleWifiRequest();
}