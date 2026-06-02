/*
  ESP32-CAM: MJPEG WiFi stream + UART image transmitter

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

#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClient.h>

#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"
#include "home_wifi_multi.h"   // # put network credentials here

#define CAM_UART_TX      14       // to Arduino UART RX
#define CAM_UART_RX      15       // to Arduino UART TX
#define UART_BAUD        115200   // Fastest reliable baud rate

// might influence frame corruption, test 150 ms
#define FRAME_INTERVAL_MS  500

static const uint8_t FRAME_START[2] = { 0xFF, 0xAA };
static const uint8_t FRAME_END[2]   = { 0xFF, 0xBB };

WebServer server(80);
bool wifiOk = false;

bool initCamera() {
  camera_config_t config;
  config.ledc_channel  = LEDC_CHANNEL_0;
  config.ledc_timer    = LEDC_TIMER_0;
  config.pin_d0        = Y2_GPIO_NUM;
  config.pin_d1        = Y3_GPIO_NUM;
  config.pin_d2        = Y4_GPIO_NUM;
  config.pin_d3        = Y5_GPIO_NUM;
  config.pin_d4        = Y6_GPIO_NUM;
  config.pin_d5        = Y7_GPIO_NUM;
  config.pin_d6        = Y8_GPIO_NUM;
  config.pin_d7        = Y9_GPIO_NUM;
  config.pin_xclk      = XCLK_GPIO_NUM;
  config.pin_pclk      = PCLK_GPIO_NUM;
  config.pin_vsync     = VSYNC_GPIO_NUM;
  config.pin_href      = HREF_GPIO_NUM;
  config.pin_sscb_sda  = SIOD_GPIO_NUM;
  config.pin_sscb_scl  = SIOC_GPIO_NUM;
  config.pin_pwdn      = PWDN_GPIO_NUM;
  config.pin_reset     = RESET_GPIO_NUM;
  config.xclk_freq_hz  = 20000000;
  config.pixel_format  = PIXFORMAT_JPEG;

  // FRAMESIZE_QQVGA (160x120) is tested and stable, FRAMESIZE_96X96 (96x96) migth work
  config.frame_size    = FRAMESIZE_QQVGA;
  config.jpeg_quality  = 20; // must be at least 20, was main cause of decoding error 
  config.fb_count      = 1;   // 1 buffer must return it before getting next
  config.fb_location   = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_LATEST;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return false;
  }
  return true;
}

void sendFrameUART(const uint8_t *data, uint32_t len) {
  Serial2.write(FRAME_START, 2);

  uint8_t lenBuf[4] = {
    (uint8_t)(len >> 24),
    (uint8_t)(len >> 16),
    (uint8_t)(len >>  8),
    (uint8_t)(len      )
  };
  Serial2.write(lenBuf, 4);
  Serial2.write(data, len);
  Serial2.write(FRAME_END, 2);
  Serial2.flush();
}

const char HEADER[]   = "HTTP/1.1 200 OK\r\n"
                        "Access-Control-Allow-Origin: *\r\n"
                        "Content-Type: multipart/x-mixed-replace; "
                        "boundary=123456789000000000000987654321\r\n";
const char BOUNDARY[] = "\r\n--123456789000000000000987654321\r\n";
const char CTNTTYPE[] = "Content-Type: image/jpeg\r\nContent-Length: ";

void handle_jpg_stream() {
  char buf[32];
  WiFiClient client = server.client();
  client.write(HEADER, strlen(HEADER));
  client.write(BOUNDARY, strlen(BOUNDARY));

  while (client.connected()) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) { delay(10); continue; }

    client.write(CTNTTYPE, strlen(CTNTTYPE));
    sprintf(buf, "%d\r\n\r\n", fb->len);
    client.write(buf, strlen(buf));
    client.write((char *)fb->buf, fb->len);
    client.write(BOUNDARY, strlen(BOUNDARY));

    esp_camera_fb_return(fb);
  }
}

void handle_jpg() {
  WiFiClient client = server.client();
  if (!client.connected()) return;

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) return;

  const char jheader[] = "HTTP/1.1 200 OK\r\n"
                         "Content-disposition: inline; filename=capture.jpg\r\n"
                         "Content-type: image/jpeg\r\n\r\n";
  client.write(jheader, strlen(jheader));
  client.write((char *)fb->buf, fb->len);

  esp_camera_fb_return(fb);
}

void handleNotFound() {
  server.send(200, "text/plain", "ESP32-CAM running");
}

void setup() {
  setCpuFrequencyMhz(80);

  Serial.begin(115200);
  Serial.println("ESP32-CAM booting...");

  Serial2.begin(UART_BAUD, SERIAL_8N1, CAM_UART_RX, CAM_UART_TX);

  if (!initCamera()) {
    Serial.println("Camera failed — halting");
    while (true) delay(1000);
  }
  Serial.println("Camera OK");

  // WiFi - try for 10 seconds, disable wifi if fails
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID1, PWD1);
  unsigned long t = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t < 10000) {
    delay(200);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    wifiOk = true;
    Serial.print("WiFi OK  IP: ");
    Serial.println(WiFi.localIP());
    server.on("/mjpeg/1", HTTP_GET, handle_jpg_stream);
    server.on("/jpg",     HTTP_GET, handle_jpg);
    server.onNotFound(handleNotFound);
    server.begin();
  } else {
    Serial.println("WiFi failed — UART only");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
  }

  Serial.println("Running — sending frames on GPIO14");
}

void loop() {
  static unsigned long lastFrame = 0;
  unsigned long now = millis();

  if (now - lastFrame >= FRAME_INTERVAL_MS) {
    lastFrame = now;

    camera_fb_t *fb = esp_camera_fb_get();
    if (fb) {
      Serial.print("Sending frame: ");
      Serial.println(fb->len);
      
      sendFrameUART(fb->buf, fb->len);
      esp_camera_fb_return(fb);
    } else {
      Serial.println("fb_get failed");
    }
  }

  if (wifiOk) {
    server.handleClient();
  }
}
