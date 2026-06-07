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
// Bluetooth Low Energy
#include <ArduinoBLE.h>
// Edge impulse inference library
#include <image_recognition_inferencing.h>

#define CAM_BAUD            230400  // must be the same as camera baud rate
#define SERIAL_BAUD         230400
#define REQ_BYTE            0x52    // 'R' - signal when ready to receive
#define FRAME_TIMEOUT_MS    2000    // try again if camera doesn't send frame
#define ENABLE_CLASSIFIER   false
#define ENABLE_SERIAL_IMAGE true
#define ENABLE_BLUETOOTH    false

#define CLASSIFIER_BUTTON_PIN 4
bool classifier_enabled = ENABLE_CLASSIFIER;
#define MASK_BUTTON_PIN       6
bool mask_mode = false;

uint32_t UART_receive_ms;
uint32_t Frame_process_ms;

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

// Maximum receive frame size: 32 KB
#define MAX_FRAME_SIZE  32768

#define FRAME_W    64
#define FRAME_H    64

static uint8_t  frameBuf[MAX_FRAME_SIZE];
static uint16_t decodeBuf[FRAME_W * FRAME_H];
static float pixelBuf[FRAME_W * FRAME_H];

// Start and END bytes to mark the image borders from the ESP32-CAM
#define START_B1  0xAA
#define START_B2  0x55
#define END_B1    0x55
#define END_B2    0xAA

// default hue values, resample with button press
uint8_t H_LO =  0, H_HI =  20;
uint8_t S_LO = 20, S_HI = 255;
uint8_t V_LO = 70, V_HI = 255;

BLEService carService("19B10000-E8F2-537E-4F6C-D104768A1214");

BLEByteCharacteristic commandCharacteristic(
  "19B10001-E8F2-537E-4F6C-D104768A1214",
  BLERead | BLENotify
);

const char* labels[] = {"go", "reverse", "left", "right", "stop"};

const uint16_t* repackFrame(const uint8_t* data) {
  for (int i = 0; i < FRAME_W * FRAME_H; i++) 
    decodeBuf[i] = ((uint16_t)data[i * 2] << 8) | ((uint16_t)data[i * 2 + 1]);
  return decodeBuf;
}

// Sends packet: 0xAA | 0x55 | image data | number of classes | scores | index
void sendToSerial(ei_impulse_result_t& result, uint8_t signal) {
  uint32_t count = FRAME_W * FRAME_H;

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

  Serial.write((uint8_t*)&signal, sizeof(uint8_t));

  Serial.write((uint8_t*)&UART_receive_ms, sizeof(uint32_t));
  Serial.write((uint8_t*)&Frame_process_ms, sizeof(uint32_t));
}

uint8_t signalPico(ei_impulse_result_t &result) {
  float best_score = 0.0f;
  const char *best_label = nullptr;

  for (size_t idx = 0; idx < EI_CLASSIFIER_LABEL_COUNT; idx++) {
    if (result.classification[idx].value > best_score) {
      best_score = result.classification[idx].value;
      best_label = result.classification[idx].label;
    }
  }

  for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    if (strcmp(best_label, labels[i]) == 0) {
      commandCharacteristic.writeValue((byte)(i + '0' + 1));
      return i + 1;
    }
  }

  return 0;
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

const float* maskFrame(const uint16_t* frame) {
  for (uint32_t i = 0; i < FRAME_W * FRAME_H; i++) {
    uint8_t h, s, v;
    rgb565ToHSV(frame[i], h, s, v);

    bool inRange = (h >= H_LO && h <= H_HI && s >= S_LO && s <= S_HI && v >= V_LO && v <= V_HI);

    pixelBuf[i] = inRange ? 255.0f : 0.0f;
  }
  return pixelBuf;
}

const float* grayscaleFrame(const uint16_t* frame) {
  for (int i = 0; i < FRAME_W * FRAME_H; i++) {

    uint16_t p = frame[i];

    uint8_t r = ((p >> 11) & 0x1F) << 3;
    uint8_t g = ((p >> 5)  & 0x3F) << 2;
    uint8_t b = ( p        & 0x1F) << 3;

    pixelBuf[i] = (r + g + b) / 3.0f * (1.0f / 255.0f);
  }

  return pixelBuf;
}

void processFrame(const uint8_t* frame, uint32_t len) {
  const uint16_t* unpacked_image = repackFrame(frame);

  const float* masked_image;
  
  if (mask_mode) masked_image = maskFrame(unpacked_image);
  else masked_image = grayscaleFrame(unpacked_image);
  
  if (classifier_enabled) {
    // Run classifier
    signal_t signal;
    numpy::signal_from_buffer(masked_image, FRAME_W * FRAME_H, &signal);
    ei_impulse_result_t result;
    EI_IMPULSE_ERROR err = run_classifier(&signal, &result, false);
    
    // If success, send image and scores trough Serial
    if (err != EI_IMPULSE_OK) {
      Serial.print("Classifier error: "); Serial.println(err);
    } else {
      uint8_t BLE_signal;
      if (ENABLE_BLUETOOTH) BLE_signal = signalPico(result);
      if (ENABLE_SERIAL_IMAGE) sendToSerial(result, BLE_signal);
      else {
        for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
          Serial.print(result.classification[i].label);
          Serial.print(": ");
          Serial.print(result.classification[i].value);
          Serial.print(" | ");
        }
        Serial.print("UART Receiving: ");
        Serial.print(UART_receive_ms);
        Serial.print(" ms | Frame Process: ");
        Serial.print(Frame_process_ms);
        Serial.println(" ms");    
      }
    }
  } else if (ENABLE_SERIAL_IMAGE) {
    ei_impulse_result_t empty_result;
    for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++)
      empty_result.classification[i].value = i/100;
    sendToSerial(empty_result, 0);
  } else {
    Serial.print("UART Receiving: ");
    Serial.print(UART_receive_ms);
    Serial.print(" ms | Frame Process: ");
    Serial.print(Frame_process_ms);
    Serial.println(" ms");
  }
}

// frame done or corrupt, restart request process
void resetRx() {
  rxState       = WAIT_START_1;
  frameLen      = 0;
  bytesRead     = 0;
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

void initBluetooth() {
  if (!BLE.begin()) {
    Serial.println("BLE failed!");
    while (1);
  }

  BLE.setLocalName("NanoCarController");
  BLE.setAdvertisedService(carService);
  carService.addCharacteristic(commandCharacteristic);
  BLE.addService(carService);
  commandCharacteristic.writeValue((byte)'s');
  BLE.advertise();
}

void setup() {
  Serial.begin(SERIAL_BAUD);  // Serial:  Writing image
  Serial1.begin(CAM_BAUD);    // Serial1: Reading image

  // Sample button setup
  pinMode(CLASSIFIER_BUTTON_PIN, INPUT_PULLUP);
  pinMode(MASK_BUTTON_PIN, INPUT_PULLUP);

  if (ENABLE_BLUETOOTH) initBluetooth();

  // Request first frame from ESP32-CAM
  Serial.println("RDY");
  requestFrame();
}

void readButton(int button_pin) {
  static bool lastButton = HIGH;
  bool button = digitalRead(button_pin);
  if (lastButton == HIGH && button == LOW) {
    switch (button_pin) {
      case CLASSIFIER_BUTTON_PIN:
        classifier_enabled = !classifier_enabled;
        break;
      case MASK_BUTTON_PIN:
        mask_mode = !mask_mode;
        break;
      default: break;
    }
  }
  lastButton = button;
}

void loop() {
  static unsigned long requestTime = millis();

  if (ENABLE_BLUETOOTH) BLE.poll();

  readButton(CLASSIFIER_BUTTON_PIN);
  readButton(MASK_BUTTON_PIN);
  
  uint32_t t0, t1, t2, t3;

  t0 = millis();
  bool frameReady = drainUART();
  t1 = millis();

  if (frameReady) {
    t2 = millis();
    processFrame(frameBuf, lastFrameLen);
    requestFrame();
    t3 = millis();

    UART_receive_ms = t1 - t0;
    Frame_process_ms = t3 - t2;
    requestTime = millis();
  } else if (millis() - requestTime > FRAME_TIMEOUT_MS) {
    Serial.println("Frame request timeout");
    requestFrame();
    requestTime = millis();
  }
}