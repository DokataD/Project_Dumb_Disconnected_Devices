#include "frame_handle.h"

static uint16_t decodeBuf[SRC_W * SRC_H];  // RGB565 96x96
static uint16_t resizeBuf[DST_W * DST_H];  // RGB565 64x64

#if CAMERA_MODE == RGB565

  // Preprocessing: uint_8 data -> uint_16
  bool unpack(const uint8_t *data, uint32_t len) {
    uint32_t pixels = len / 2;

    for (uint32_t i = 0; i < pixels; i++) {
      decodeBuf[i] = ((uint16_t)data[i * 2] << 8) | (uint16_t)data[i * 2 + 1];
    }

    return true;
  }

#else
  
  // JPEG decoder
  JPEGDEC jpeg;

  // Preprocessing: raw JPEG -> decoded RGB565
  int jpegDrawCallback(JPEGDRAW *pDraw) {
    uint16_t *src = pDraw->pPixels;
    for (int row = 0; row < pDraw->iHeight; row++) {
      int y = pDraw->y + row;
      if (y >= SRC_H) break;
      for (int col = 0; col < pDraw->iWidth; col++) {
        int x = pDraw->x + col;
        if (x >= SRC_W) break;
        decodeBuf[y * SRC_W + x] = src[row * pDraw->iWidth + col];
      }
    }
    return 1;
  }

  bool unpack(const uint8_t *data, uint32_t len) {
    // Decode JPEG
    if (!jpeg.openRAM((uint8_t *)data, (int)len, jpegDrawCallback)) {
      Serial.println("JPEGDEC openRAM failed");
      return false;
    }
    if (!jpeg.decode(0, 0, 0)) {
      Serial.println("JPEGDEC decode failed");
      jpeg.close();
      return false;
    }
    jpeg.close();
    return true;
  }

#endif

camera_fb_t* getFrame() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) Serial.println("fb_get failed");
  return fb;
}

void ConvertResize(const uint8_t *data, uint32_t len) {
  bool unpack_success = unpack(data, len);
  if (!unpack_success) return;

  for (int y = 0; y < DST_H; y++) {

    int srcY = (y * SRC_H) / DST_H;
    if (srcY >= SRC_H) srcY = SRC_H - 1;

    for (int x = 0; x < DST_W; x++) {

      int srcX = (x * SRC_W) / DST_W;
      if (srcX >= SRC_W) srcX = SRC_W - 1;

      resizeBuf[y * DST_W + x] = decodeBuf[srcY * SRC_W + srcX];
    }
  }
}

// Sends packet: Start byte 1 | Start byte 2 | image data | End byte 1 | End byte 2 
void sendToSerial() {

  uint32_t pixels = DST_W * DST_H;
  uint32_t byteLen = pixels * 2;

  Serial2.write(START_B1);
  Serial2.write(START_B2);

  // length (bytes, big-endian)
  Serial2.write((byteLen >> 24) & 0xFF);
  Serial2.write((byteLen >> 16) & 0xFF);
  Serial2.write((byteLen >>  8) & 0xFF);
  Serial2.write( byteLen        & 0xFF);

  // send RGB565 raw
  for (uint32_t i = 0; i < pixels; i++) {
    Serial2.write((uint8_t)(resizeBuf[i] >> 8));   // MSB
    Serial2.write((uint8_t)(resizeBuf[i] & 0xFF)); // LSB
  }

  Serial2.write(END_B1);
  Serial2.write(END_B2);
}

void handleFrameRequest() {
  camera_fb_t *fb = getFrame();
  if (!fb) {
    Serial.println("fb_get failed");
    return;
  }

  ConvertResize(fb->buf, fb->len);
  sendToSerial();
  esp_camera_fb_return(fb);
  
  Serial.print("Sending frame: ");
  Serial.println(fb->len);
}
