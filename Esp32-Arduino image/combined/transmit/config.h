#ifndef CONFIG_H
#define CONFIG_H

  #include <Arduino.h>
  #include "esp_camera.h"

  #define UART_BAUD         230400    // Fastest reliable baud rate
  #define CAM_UART_TX       14        // to Arduino UART RX
  #define CAM_UART_RX       15        // to Arduino UART TX
  #define REQ_BYTE          0x52      // 'R' - Arduino signal when ready to receive
  #define CAMERA_MODEL_AI_THINKER     // Camera model
  #define ENABLE_WIFI       false     // Disabling wifi increases speed
  #define CAMERA_QUALITY    15        // 20 is tested stable, 15 is probably highest
  
  // Mode selector, define the current camera model in use
  #define JPEG              0
  #define RGB565            1
  #define CAMERA_MODE       RGB565

  #ifndef CAMERA_MODE
    #error "Capture mode not selected"
  #endif

  #ifndef CAMERA_MODEL_AI_THINKER
    #error "Camera model not selected"
  #else
    #define PWDN_GPIO_NUM     32
    #define RESET_GPIO_NUM    -1
    #define XCLK_GPIO_NUM      0
    #define SIOD_GPIO_NUM     26
    #define SIOC_GPIO_NUM     27

    #define Y9_GPIO_NUM       35
    #define Y8_GPIO_NUM       34
    #define Y7_GPIO_NUM       39
    #define Y6_GPIO_NUM       36
    #define Y5_GPIO_NUM       21
    #define Y4_GPIO_NUM       19
    #define Y3_GPIO_NUM       18
    #define Y2_GPIO_NUM        5
    #define VSYNC_GPIO_NUM    25
    #define HREF_GPIO_NUM     23
    #define PCLK_GPIO_NUM     22
  #endif

  bool initCamera();

#endif