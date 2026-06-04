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

#include "esp_camera.h"

#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

#define CAM_UART_TX      14       // to Arduino UART RX
#define CAM_UART_RX      15       // to Arduino UART TX
#define UART_BAUD        230400   // Fastest reliable baud rate
#define REQ_BYTE         0x52     // 'R' - Arduino signal when ready to receive

// Start and END bytes to mark the image borders to Arduino
static const uint8_t FRAME_START[2] = { 0xFF, 0xAA };
static const uint8_t FRAME_END[2]   = { 0xFF, 0xBB };

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
  // this camera doesnt support jpeg
  config.pixel_format  = PIXFORMAT_RGB565;

  // FRAMESIZE_96X96 works with RGB565
  config.frame_size    = FRAMESIZE_96X96;
  config.jpeg_quality  = 20;  // must be at least 20, was main cause of decoding error
  config.fb_count      = 1;   // 1 buffer, multiple causes framebuffer overflow (FB-OVF)
  config.fb_location   = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_LATEST;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return false;
  }
  return true;
}

// Transmit to Arduino: Start byte - Image data - End byte
void sendFrameUART(const uint8_t *data, uint32_t len) {
  Serial2.write(FRAME_START, 2);

  uint8_t lenBuf[4] = {
    (uint8_t)(len >> 24),
    (uint8_t)(len >> 16),
    (uint8_t)(len >>  8),
    (uint8_t)(len      )
  };
  
  Serial.print("Sending frame: ");
  Serial.println(len);
  
  Serial2.write(lenBuf, 4);
  Serial2.write(data, len);
  Serial2.write(FRAME_END, 2);
  Serial2.flush();
}

// Triggered when receives request byte 'R'
void handleFrameRequest() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("fb_get failed");
    return;
  }
  // Transmit to Arduino
  sendFrameUART(fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

void setup() {
  setCpuFrequencyMhz(80);

  Serial.begin(115200);
  Serial.println("ESP32-CAM booting ...");

  Serial2.begin(UART_BAUD, SERIAL_8N1, CAM_UART_RX, CAM_UART_TX);

  if (!initCamera()) {
    Serial.println("Camera failed - halting");
    while (true) delay(1000);
  }
  Serial.println("Camera OK");
  Serial.println("Running - sending frames on GPIO14");
}

void loop() {
  if (Serial2.available()) {
    uint8_t b = Serial2.read();
    if (b == REQ_BYTE) {
      handleFrameRequest();
    }
  }
}