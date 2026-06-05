#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

#define UART_BAUD        230400   // Fastest reliable baud rate
#define CAM_UART_TX      14       // to Arduino UART RX
#define CAM_UART_RX      15       // to Arduino UART TX
#define REQ_BYTE         0x52     // 'R' - Arduino signal when ready to receive
#define CAMERA_MODEL_AI_THINKER   // Camera model

// Start and END bytes to mark the image borders to Arduino
static const uint8_t FRAME_START[2] = { 0xFF, 0xAA };
static const uint8_t FRAME_END[2]   = { 0xFF, 0xBB };

#endif