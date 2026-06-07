#ifndef FRAME_HANDLE_H
#define FRAME_HANDLE_H

#include <Arduino.h>
#include "config.h"
#include "esp_camera.h"

#if CAMERA_MODE == RGB565

  #define SRC_W     96
  #define SRC_H     96

#else

  #include <JPEGDEC.h>

  #define SRC_W     160
  #define SRC_H     120

  int jpegDrawCallback(JPEGDRAW *pDraw);

#endif

#define DST_W     64
#define DST_H     64

#define START_B1  0xAA
#define START_B2  0x55
#define END_B1    0x55
#define END_B2    0xAA

bool unpack(const uint8_t *data, uint32_t len);

camera_fb_t* getFrame();

void ConvertResize(const uint8_t *data, uint32_t len);

void sendToSerial();

void handleFrameRequest();

#endif