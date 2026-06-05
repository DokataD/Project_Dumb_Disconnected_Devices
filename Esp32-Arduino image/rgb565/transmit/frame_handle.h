#ifndef FRAME_HANDLE_H
#define FRAME_HANDLE_H

#include "config.h"
#include "esp_camera.h"

camera_fb_t* getFrame();

void sendFrameUART(const uint8_t *data, uint32_t len);

void handleFrameRequest();

#endif