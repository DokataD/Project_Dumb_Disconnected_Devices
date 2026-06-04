/*
  Arduino Nano 33 BLE — UART JPEG frame receiver
  
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
// Edge impulse inference library
#include <image_recognition_inferencing.h>

#define CAM_BAUD          230400  // must be the same as camera baud rate
#define REQ_BYTE          0x52    // 'R' - signal when ready to receive
#define FRAME_TIMEOUT_MS  2000    // try again if camera doesn't send frame
#define SERIAL_DEBUG      false   // True: Send debug image to Serial - False: run classifier and print result

enum RxState {
  WAIT_START_1,   // Looking for 0xFF
  WAIT_START_2,   // Looking for 0xAA
  READ_LEN_0,     // MSB of length
  READ_LEN_1,
  READ_LEN_2,
  READ_LEN_3,     // LSB of length
  READ_PAYLOAD,   // Accumulating JPEG bytes
  WAIT_END_1,     // Looking for 0xFF
  WAIT_END_2      // Looking for 0xBB
};

static RxState  rxState       = WAIT_START_1;
static uint32_t frameLen      = 0;
static uint32_t lastFrameLen  = 0;
static uint32_t bytesRead     = 0;
static bool     frameReady    = false;

#define SRC_W     96      // ESP32-CAM JPEG stream size - Width
#define SRC_H     96      // ESP32-CAM JPEG stream size - Height

#define DST_W     64      // Resize target after preprocessing and classifier input size
#define DST_H     64      // Resize target after preprocessing and classifier input size

#define BUTTON_PIN 4      // Skin hue sample button
bool HSV_button = false;

// default hue values, resample with button press
uint8_t H_LO =  0, H_HI =  20;
uint8_t S_LO = 20, S_HI = 255;
uint8_t V_LO = 70, V_HI = 255;

// Maximum receive frame size: 32 KB
#define MAX_FRAME_SIZE  32768

// Start and END bytes to mark the image borders from the ESP32-CAM
#define START_B1  0xFF
#define START_B2  0xAA
#define END_B1    0xFF
#define END_B2    0xBB

static uint8_t  frameBuf[MAX_FRAME_SIZE];       // ESP32-CAM  96x96
static uint16_t decodeBuf[SRC_W * SRC_H];       // RGB565     96x96
static float    pixelBuf[DST_W * DST_H];        // float mask 64x64

// Preprocessing: uint_8 data -> uint_16
void unpackRGB565(uint32_t len) {
  uint32_t pixels = len / 2;

  for (uint32_t i = 0; i < pixels; i++) {
    decodeBuf[i] = ((uint16_t)frameBuf[i * 2] << 8) | (uint16_t)frameBuf[i * 2 + 1];
  }
}

// Preprocessing: RGB565 -> HSV mask
void rgb565ToHSV(uint16_t rgb565, uint8_t &h, uint8_t &s, uint8_t &v) {
  float r = ( rgb565 >> 11)         * (1.0f / 31.0f);
  float g = ((rgb565 >>  5) & 0x3F) * (1.0f / 63.0f);
  float b = ( rgb565        & 0x1F) * (1.0f / 31.0f);

  float cmax  = max(r, max(g, b));
  float cmin  = min(r, min(g, b));
  float delta = cmax - cmin;

  // Value
  v = (uint8_t)(cmax * 255.0f);
  // Saturation
  s = (cmax > 0.0f) ? (uint8_t)((delta / cmax) * 255.0f) : 0;
  // Hue
  float hf = 0.0f;
  if (delta > 1e-6f) {
    if      (cmax == r) hf = 60.0f * fmodf((g - b) / delta, 6.0f);
    else if (cmax == g) hf = 60.0f * ((b - r) / delta + 2.0f);
    else                hf = 60.0f * ((r - g) / delta + 4.0f);
    if (hf < 0.0f) hf += 180.0f;
  }
  h = (uint8_t)(hf / 2.0f);
}

// Preprocessing: HSV 96x96 -> float mask 64x64
void resizeAndMask() {
  for (int y = 0; y < DST_H; y++) {
    int srcY = (int)(y * SRC_H / (float)DST_H);
    if (srcY >= SRC_H) srcY = SRC_H - 1;

    for (int x = 0; x < DST_W; x++) {
      int srcX = (int)(x * SRC_W / (float)DST_W);
      if (srcX >= SRC_W) srcX = SRC_W - 1;

      uint8_t h, s, v;
      rgb565ToHSV(decodeBuf[srcY * SRC_W + srcX], h, s, v);

      bool inRange = (h >= H_LO && h <= H_HI && s >= S_LO && s <= S_HI && v >= V_LO && v <= V_HI);

      pixelBuf[y * DST_W + x] = inRange ? 255.0f : 0.0f;
    }
  }
}

// Sends packet: 0xAA | 0x55 | image data | number of classes | scores | index
void sendToSerial(ei_impulse_result_t &result) {
  uint32_t count = DST_W * DST_H;

  Serial.write(0xAA);
  Serial.write(0x55);

  // image size
  Serial.write((uint8_t*)&count, sizeof(count));
  // image data
  Serial.write((uint8_t*)pixelBuf, count * sizeof(float));

  // number of classes
  uint8_t n = EI_CLASSIFIER_LABEL_COUNT;
  Serial.write(n);

  // send scores
  for (size_t i = 0; i < n; i++) {
    // score
    float score = result.classification[i].value;
    Serial.write((uint8_t*)&score, sizeof(float));
    // index
    uint8_t idx = i;
    Serial.write(&idx, 1);
  }
}
  
void processFrame(const uint8_t *data, uint32_t len) {
  // Decode
  unpackRGB565(len);
  // Apply preprocessing
  resizeAndMask();

  // Run classifier
  signal_t signal;
  numpy::signal_from_buffer(pixelBuf, DST_W * DST_H, &signal);
  ei_impulse_result_t result;
  EI_IMPULSE_ERROR err = run_classifier(&signal, &result, false);
  
  // If success, send image and scores trough Serial
  if (err != EI_IMPULSE_OK) {
    Serial.print("Classifier error: "); Serial.println(err);
  } else {
    sendToSerial(result);
  }
}

// On button press, adjust HSV values to range from current image
void calibrateSkin() {
  // Sample box in center of the image
  const int cx = SRC_W / 2;
  const int cy = SRC_H / 2;
  const int radius = 10;

  uint32_t hSum = 0;
  uint32_t sSum = 0;
  uint32_t vSum = 0;
  uint32_t count = 0;

  for (int y = cy - radius; y <= cy + radius; y++) {
    for (int x = cx - radius; x <= cx + radius; x++) {
      uint8_t h, s, v;
      rgb565ToHSV(decodeBuf[y * SRC_W + x], h, s, v);

      hSum += h;
      sSum += s;
      vSum += v;
      count++;
    }
  }

  uint8_t hMean = hSum / count;
  uint8_t sMean = sSum / count;
  uint8_t vMean = vSum / count;

  // Tunable margins
  const int H_MARGIN = 12;
  const int S_MARGIN = 50;
  const int V_MARGIN = 50;

  H_LO = max(0,   (int)hMean - H_MARGIN);
  H_HI = min(180, (int)hMean + H_MARGIN);

  S_LO = max(0,   (int)sMean - S_MARGIN);
  S_HI = min(255, (int)sMean + S_MARGIN);

  V_LO = max(0,   (int)vMean - V_MARGIN);
  V_HI = min(255, (int)vMean + V_MARGIN);
}

// frame done or corrupt, restart request process
void resetRx() {
  rxState   = WAIT_START_1;
  frameLen  = 0;
  bytesRead = 0;
}

// Send request byte to ESP32-CAM to signal ready for new frame
void requestFrame() {
  // make sure TX is empty before sending
  Serial1.flush();
  // Reset receiving state 
  resetRx();
  // drain any stale RX bytes
  while (Serial1.available()) Serial1.read();
  // Send request byte
  Serial1.write(REQ_BYTE);
}

// Read ESP32-CAM stream
bool drainUART() {
  // Receive until stream stops
  while (Serial1.available()) {
    uint8_t b = Serial1.read();
    
    switch (rxState) {
      // Check for Frame Start first byte
      case WAIT_START_1:
        if (b == START_B1) rxState = WAIT_START_2;
        break;
      // Check for Frame Start second byte
      case WAIT_START_2:
        if      (b == START_B2) { rxState = READ_LEN_0; frameLen = bytesRead = 0; }
        else if (b == START_B1)   rxState = WAIT_START_2;
        else                      rxState = WAIT_START_1;
        break;
      // Read frame lenght from Most- to Least Significant Bit
      case READ_LEN_0: frameLen  = ((uint32_t)b << 24); rxState = READ_LEN_1; break;
      case READ_LEN_1: frameLen |= ((uint32_t)b << 16); rxState = READ_LEN_2; break;
      case READ_LEN_2: frameLen |= ((uint32_t)b <<  8); rxState = READ_LEN_3; break;
      case READ_LEN_3:
        frameLen |= b;
        if (frameLen == 0 || frameLen > MAX_FRAME_SIZE) { resetRx(); }
        else rxState = READ_PAYLOAD;
        break;
      // All checks complete, start receiving image data
      case READ_PAYLOAD:
        frameBuf[bytesRead++] = b;
        if (bytesRead >= frameLen) rxState = WAIT_END_1;
        break;
      // Frame end byte received or ran out of frame space, check which is the case
      case WAIT_END_1:
        rxState = (b == END_B1) ? WAIT_END_2 : WAIT_START_1;
        break;
      // Frame end first byte was received, check for Frame end second byte
      case WAIT_END_2:
        lastFrameLen = frameLen;
        resetRx();
        if (b == END_B2) return true;   // complete valid frame
        break;
      // Unhandled state, restart
      default: resetRx(); break;
    }
  }
  return false;
}

void setup() {
  Serial.begin(115200);     // Serial:  Writing image
  Serial1.begin(CAM_BAUD);  // Serial1: Reading image

  // Sample button setup
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Request first frame from ESP32-CAM
  Serial.println("RDY");
  requestFrame();
}

void loop() {
  static unsigned long requestTime = millis();

  // Read Sample button
  static bool lastButton = HIGH;
  bool button = digitalRead(BUTTON_PIN);
  if (lastButton == HIGH && button == LOW) calibrateSkin();
  lastButton = button;

  if (drainUART()) {
    // Complete frame received, process it
    processFrame(frameBuf, lastFrameLen);
    // Request next frame immediately after processing
    requestFrame();
    requestTime = millis();
  } else if (millis() - requestTime > FRAME_TIMEOUT_MS) {
    // No frame arrived in time, request again
    Serial.println("Frame request timeout");
    requestFrame();
    requestTime = millis();
  }
}
