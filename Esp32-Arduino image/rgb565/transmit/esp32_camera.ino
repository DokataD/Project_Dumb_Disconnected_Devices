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
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClient.h>
#include "esp_camera.h"

#include "config.h"
#include "camera_pins.h"
// #include "wifi_handle.h"
#include "frame_handle.h"
#include "wifi_credentials.h"

const char HEADER[]   = "HTTP/1.1 200 OK\r\n"
                        "Access-Control-Allow-Origin: *\r\n"
                        "Content-Type: multipart/x-mixed-replace; "
                        "boundary=123456789000000000000987654321\r\n";
const char BOUNDARY[] = "\r\n--123456789000000000000987654321\r\n";
const char CTNTTYPE[] = "Content-Type: image/jpeg\r\nContent-Length: ";

WebServer server(80);
bool wifiOk = false;

bool initWifi() {
  bool wifiOk = false;
  // WiFi - try for 10 seconds, disable wifi streaming if it fails
  WiFi.mode(WIFI_STA);
  // credentials from home_wifi_multi.h
  WiFi.begin(SSID1, PWD1);
  unsigned long t = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t < 20000) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    wifiOk = true;
    Serial.print("WiFi OK - IP: ");
    Serial.println(WiFi.localIP());
    server.on("/mjpeg/1", HTTP_GET, handleJPGStream);
    server.onNotFound(handleNotFound);
    server.begin();
  } else {
    wifiOk = false;
    Serial.println("WiFi failed - UART only");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
  }
 
 return wifiOk;
}

void handleJPGStream() {
  char buf[32];
  WiFiClient client = server.client();
  client.write(HEADER, strlen(HEADER));
  client.write(BOUNDARY, strlen(BOUNDARY));  

  while (client.connected()) {

    camera_fb_t *fb = getFrame();
    if (!fb) continue;

    uint8_t *jpeg_buf = NULL;
    size_t jpeg_len = 0;

    bool jpg_conv_ok = frame2jpg(fb, 10, &jpeg_buf, &jpeg_len);

    esp_camera_fb_return(fb);

    if (!jpg_conv_ok || !jpeg_buf) continue;

    client.write(CTNTTYPE, strlen(CTNTTYPE));
    sprintf(buf, "%d\r\n\r\n", jpeg_len);
    client.write(buf, strlen(buf));
    client.write((char *)jpeg_buf, jpeg_len);
    client.write(BOUNDARY, strlen(BOUNDARY));

    free(jpeg_buf);

    delay(30);  
  }
}

// Wifi streaming function
void handleNotFound() {
  server.send(200, "text/plain", "ESP32-CAM running");
}

void setup() {
  // set to 240 if slow
  setCpuFrequencyMhz(80);

  Serial.begin(115200);
  Serial.println("ESP32-CAM booting ...");

  Serial2.begin(UART_BAUD, SERIAL_8N1, CAM_UART_RX, CAM_UART_TX);

  if (!initCamera()) {
    Serial.println("Camera failed");
    while (true) delay(1000);
  }
  Serial.println("Camera OK");
  
  wifiOk = initWifi();

  Serial.println("Sending frames on GPIO14");
}

void loop() {
  if (Serial2.available()) {
    uint8_t b = Serial2.read();
    if (b == REQ_BYTE) handleFrameRequest();
  }

  if (wifiOk) {
    server.handleClient();
  }
}