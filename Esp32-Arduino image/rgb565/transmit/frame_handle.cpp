#include "frame_handle.h"

camera_fb_t* getFrame() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) Serial.println("fb_get failed");
  return fb;
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

void handleFrameRequest() {
  camera_fb_t *fb = getFrame();
  if (!fb) return;

  sendFrameUART(fb->buf, fb->len);
  esp_camera_fb_return(fb);
}
