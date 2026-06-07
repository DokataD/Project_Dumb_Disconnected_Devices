#include "wifi_handle.h"

WebServer server(80);
bool wifiOk = false;

void initWifi() {
  // WiFi - try for 10 seconds, disable wifi streaming if it fails
  WiFi.mode(WIFI_STA);
  // credentials from home_wifi_multi.h
  WiFi.begin(SSID1, PWD1);
  unsigned long t = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t < 15000) {
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
}

void handleJPGStream() {
  char buf[32];
  WiFiClient client = server.client();
  client.write(HEADER, strlen(HEADER));
  client.write(BOUNDARY, strlen(BOUNDARY));  

  while (client.connected()) {
    camera_fb_t *fb = getFrame();
    if (!fb) continue;

    if (CAMERA_MODE == RGB565) {
      uint8_t *jpegBuf = NULL;
      size_t jpegLen = 0;
      bool jpg_conv_ok = frame2jpg(fb, 10, &jpegBuf, &jpegLen);
      esp_camera_fb_return(fb);
      if (!jpg_conv_ok || !jpegBuf) continue;

      client.write(CTNTTYPE, strlen(CTNTTYPE));
      sprintf(buf, "%d\r\n\r\n", jpegLen);
      client.write(buf, strlen(buf));
      client.write((char *)jpegBuf, jpegLen);
      client.write(BOUNDARY, strlen(BOUNDARY));

      free(jpegBuf);
      delay(30);
    } else {
      client.write(CTNTTYPE, strlen(CTNTTYPE));
      sprintf(buf, "%d\r\n\r\n", fb->len);
      client.write(buf, strlen(buf));
      client.write((char *)fb->buf, fb->len);
      client.write(BOUNDARY, strlen(BOUNDARY));

      esp_camera_fb_return(fb);
    }
  }
}

void handleNotFound() {
  server.send(200, "text/plain", "ESP32-CAM running");
}

void handleWifiRequest() {
  if (wifiOk) {
    server.handleClient();
  }
}