#include <Arduino.h>
#include <JPEGDEC.h>
#include <ArduinoBLE.h>
#include <image_recognition_v2_inferencing.h>

#define CAM_BAUD 115200
#define REQ_BYTE 0x52
#define FRAME_TIMEOUT_MS 2000

#define SERVICE_UUID "19B10000-E8F2-537E-4F6C-D104768A1214"
#define CHARACTERISTIC_UUID "19B10001-E8F2-537E-4F6C-D104768A1214"
BLEService carService(SERVICE_UUID);
BLEByteCharacteristic commandChar(CHARACTERISTIC_UUID, BLERead | BLENotify);

// ── Gesture logic tuning ──
#define WINDOW 7       // sliding window size
#define VOTE_NEEDED 5  // votes within window to confirm a command
#define STOP_NEEDED 5  // "none" frames within window to auto-stop
#define CONFIDENCE_THRESHOLD 0.70f
#define MIN_SEND_INTERVAL_MS 1000  // at most one command per second
#define SEND_DEBUG 1               // 1 = stream mask to viewer; 0 = faster driving

#define SRC_W 160
#define SRC_H 120
#define DST_W 96
#define DST_H 96
#define NPIX (DST_W * DST_H)
#define MAX_FRAME_SIZE 8192

uint8_t H_LO = 0, H_HI = 25;
uint8_t S_LO = 20, S_HI = 255;
uint8_t V_LO = 30, V_HI = 255;

#define START_B1 0xFF
#define START_B2 0xAA
#define END_B1 0xFF
#define END_B2 0xBB

enum RxState { WAIT_S1,
               WAIT_S2,
               LEN0,
               LEN1,
               LEN2,
               LEN3,
               PAYLOAD,
               END1,
               END2 };
static RxState rxState = WAIT_S1;
static uint32_t frameLen = 0, lastFrameLen = 0, bytesRead = 0;

static uint8_t frameBuf[MAX_FRAME_SIZE];
static uint16_t decodeBuf[SRC_W * SRC_H];
static uint8_t maskBuf[NPIX];
static uint8_t tmpBuf[NPIX];
static uint16_t flood_stack[NPIX];

// ── Vote window ──
char window[WINDOW] = { 0 };
int widx = 0;
char lastSent = 's';
unsigned long lastSendTime = 0;

JPEGDEC jpeg;

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

void rgb565ToHSV(uint16_t px, uint8_t &h, uint8_t &s, uint8_t &v) {
  float r = ((px >> 11) & 0x1F) * (1.0f / 31.0f);
  float g = ((px >> 5) & 0x3F) * (1.0f / 63.0f);
  float b = (px & 0x1F) * (1.0f / 31.0f);
  float cmax = max(r, max(g, b)), cmin = min(r, min(g, b)), d = cmax - cmin;
  v = (uint8_t)(cmax * 255.0f);
  s = (cmax > 0) ? (uint8_t)((d / cmax) * 255.0f) : 0;
  float hf = 0;
  if (d > 1e-6f) {
    if (cmax == r) hf = 30.0f * fmodf((g - b) / d, 6.0f);
    else if (cmax == g) hf = 30.0f * ((b - r) / d + 2.0f);
    else hf = 30.0f * ((r - g) / d + 4.0f);
    if (hf < 0) hf += 180.0f;
  }
  h = (uint8_t)hf;
}

void buildMask() {
  for (int y = 0; y < DST_H; y++) {
    int sy = (int)(y * SRC_H / (float)DST_H);
    if (sy >= SRC_H) sy = SRC_H - 1;
    for (int x = 0; x < DST_W; x++) {
      int sx = (int)(x * SRC_W / (float)DST_W);
      if (sx >= SRC_W) sx = SRC_W - 1;
      uint8_t h, s, v;
      rgb565ToHSV(decodeBuf[sy * SRC_W + sx], h, s, v);
      bool skin = (h >= H_LO && h <= H_HI && s >= S_LO && s <= S_HI && v >= V_LO && v <= V_HI);
      maskBuf[y * DST_W + x] = skin ? 255 : 0;
    }
  }
}

void despeckle() {
  for (int i = 0; i < NPIX; i++) tmpBuf[i] = maskBuf[i];
  for (int y = 0; y < DST_H; y++)
    for (int x = 0; x < DST_W; x++) {
      if (maskBuf[y * DST_W + x]) {
        int n = 0;
        if (x > 0 && maskBuf[y * DST_W + x - 1]) n++;
        if (x < DST_W - 1 && maskBuf[y * DST_W + x + 1]) n++;
        if (y > 0 && maskBuf[(y - 1) * DST_W + x]) n++;
        if (y < DST_H - 1 && maskBuf[(y + 1) * DST_W + x]) n++;
        if (n < 2) tmpBuf[y * DST_W + x] = 0;
      }
    }
  for (int i = 0; i < NPIX; i++) maskBuf[i] = tmpBuf[i];
}

void keepLargestBlob() {
  for (int i = 0; i < NPIX; i++) tmpBuf[i] = 0;
  int bestSeed = -1;
  uint32_t bestSize = 0;
  for (int start = 0; start < NPIX; start++) {
    if (maskBuf[start] != 255 || tmpBuf[start]) continue;
    uint32_t size = 0;
    int sp = 0;
    flood_stack[sp++] = start;
    tmpBuf[start] = 1;
    while (sp > 0) {
      uint16_t p = flood_stack[--sp];
      size++;
      int x = p % DST_W, y = p / DST_W;
      if (x > 0) {
        int n = p - 1;
        if (maskBuf[n] == 255 && !tmpBuf[n]) {
          tmpBuf[n] = 1;
          flood_stack[sp++] = n;
        }
      }
      if (x < DST_W - 1) {
        int n = p + 1;
        if (maskBuf[n] == 255 && !tmpBuf[n]) {
          tmpBuf[n] = 1;
          flood_stack[sp++] = n;
        }
      }
      if (y > 0) {
        int n = p - DST_W;
        if (maskBuf[n] == 255 && !tmpBuf[n]) {
          tmpBuf[n] = 1;
          flood_stack[sp++] = n;
        }
      }
      if (y < DST_H - 1) {
        int n = p + DST_W;
        if (maskBuf[n] == 255 && !tmpBuf[n]) {
          tmpBuf[n] = 1;
          flood_stack[sp++] = n;
        }
      }
    }
    if (size > bestSize) {
      bestSize = size;
      bestSeed = start;
    }
  }
  if (bestSeed < 0) return;
  for (int i = 0; i < NPIX; i++) tmpBuf[i] = 0;
  int sp = 0;
  flood_stack[sp++] = bestSeed;
  tmpBuf[bestSeed] = 255;
  while (sp > 0) {
    uint16_t p = flood_stack[--sp];
    int x = p % DST_W, y = p / DST_W;
    if (x > 0) {
      int n = p - 1;
      if (maskBuf[n] == 255 && !tmpBuf[n]) {
        tmpBuf[n] = 255;
        flood_stack[sp++] = n;
      }
    }
    if (x < DST_W - 1) {
      int n = p + 1;
      if (maskBuf[n] == 255 && !tmpBuf[n]) {
        tmpBuf[n] = 255;
        flood_stack[sp++] = n;
      }
    }
    if (y > 0) {
      int n = p - DST_W;
      if (maskBuf[n] == 255 && !tmpBuf[n]) {
        tmpBuf[n] = 255;
        flood_stack[sp++] = n;
      }
    }
    if (y < DST_H - 1) {
      int n = p + DST_W;
      if (maskBuf[n] == 255 && !tmpBuf[n]) {
        tmpBuf[n] = 255;
        flood_stack[sp++] = n;
      }
    }
  }
  for (int i = 0; i < NPIX; i++) maskBuf[i] = tmpBuf[i];
}
void closeOnce() {
  // dilate
  for (int y = 0; y < DST_H; y++)
    for (int x = 0; x < DST_W; x++) {
      uint8_t v = maskBuf[y * DST_W + x];
      if (!v && x > 0 && x < DST_W - 1 && y > 0 && y < DST_H - 1) {
        if (maskBuf[y * DST_W + x - 1] || maskBuf[y * DST_W + x + 1] || maskBuf[(y - 1) * DST_W + x] || maskBuf[(y + 1) * DST_W + x]) v = 255;
      }
      tmpBuf[y * DST_W + x] = v;
    }
  // erode back
  for (int y = 0; y < DST_H; y++)
    for (int x = 0; x < DST_W; x++) {
      uint8_t v = tmpBuf[y * DST_W + x];
      if (v && x > 0 && x < DST_W - 1 && y > 0 && y < DST_H - 1) {
        if (!tmpBuf[y * DST_W + x - 1] || !tmpBuf[y * DST_W + x + 1] || !tmpBuf[(y - 1) * DST_W + x] || !tmpBuf[(y + 1) * DST_W + x]) v = 0;
      } else v = 0;
      maskBuf[y * DST_W + x] = v;
    }
}
void cleanMask() {
  despeckle();
  keepLargestBlob();
}

int get_signal_data(size_t offset, size_t length, float *out) {
  for (size_t i = 0; i < length; i++) {
    uint8_t m = maskBuf[offset + i];
    out[i] = (float)(((uint32_t)m << 16) | ((uint32_t)m << 8) | m);
  }
  return 0;
}

#if SEND_DEBUG
void sendDebug(const char *label, float conf) {
  Serial.write(0xAA);
  Serial.write(0x55);
  uint32_t count = NPIX;
  Serial.write((uint8_t *)&count, 4);
  Serial.write(maskBuf, NPIX);
  uint8_t len = strlen(label);
  Serial.write(len);
  Serial.write((const uint8_t *)label, len);
  Serial.write((uint8_t *)&conf, 4);
}
#endif

// forward->'1', left->'3', right->'4', stop->'s', back/unknown ignored
char gestureToCommand(const char *label) {
  if (!strcmp(label, "forward")) return '1';
  if (!strcmp(label, "back")) return '2';
  if (!strcmp(label, "left")) return '3';
  if (!strcmp(label, "right")) return '4';
  if (!strcmp(label, "stop")) return 's';
  return 0;
}

void handleGesture(const char *label, float conf) {
  // weak prediction or "back" -> treated as "none" (0)
  char cmd = (conf >= CONFIDENCE_THRESHOLD) ? gestureToCommand(label) : 0;

  window[widx] = cmd;
  widx = (widx + 1) % WINDOW;

  int c1 = 0, c2 = 0, c3 = 0, c4 = 0, cs = 0, cnone = 0;
  for (int i = 0; i < WINDOW; i++) {
    switch (window[i]) {
      case '1': c1++; break;
      case '2': c2++; break;
      case '3': c3++; break;
      case '4': c4++; break;
      case 's': cs++; break;
      default: cnone++; break;
    }
  }

  char best = 0;
  int bestc = 0;
  if (c1 > bestc) {
    bestc = c1;
    best = '1';
  }
  if (c2 > bestc) {
    bestc = c2;
    best = '2';
  }
  if (c3 > bestc) {
    bestc = c3;
    best = '3';
  }
  if (c4 > bestc) {
    bestc = c4;
    best = '4';
  }
  if (cs > bestc) {
    bestc = cs;
    best = 's';
  }

  unsigned long now = millis();
  if (bestc >= VOTE_NEEDED) {
    if (best != lastSent && (now - lastSendTime) >= MIN_SEND_INTERVAL_MS) {
      commandChar.writeValue((uint8_t)best);
      lastSent = best;
      lastSendTime = now;
    }
  } else if (cnone >= STOP_NEEDED) {
    if (lastSent != 's') {  // auto-stop when hand is lost
      commandChar.writeValue((uint8_t)'s');
      lastSent = 's';
      lastSendTime = now;
    }
  }
}

void processFrame(const uint8_t *data, uint32_t len) {
  if (!jpeg.openRAM((uint8_t *)data, (int)len, jpegDrawCallback)) return;
  if (!jpeg.decode(0, 0, 0)) {
    jpeg.close();
    return;
  }
  jpeg.close();
  buildMask();
  cleanMask();
  signal_t signal;
  signal.total_length = NPIX;
  signal.get_data = &get_signal_data;
  ei_impulse_result_t result;
  if (run_classifier(&signal, &result, false) != EI_IMPULSE_OK) return;
  int best = 0;
  float bestv = 0;
  for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    if (result.classification[i].value > bestv) {
      bestv = result.classification[i].value;
      best = i;
    }
  }
  const char *label = ei_classifier_inferencing_categories[best];
#if SEND_DEBUG
  sendDebug(label, bestv);
#endif
  handleGesture(label, bestv);
}

void resetRx() {
  rxState = WAIT_S1;
  frameLen = 0;
  bytesRead = 0;
}
void requestFrame() {
  Serial1.flush();
  resetRx();
  while (Serial1.available()) Serial1.read();
  Serial1.write(REQ_BYTE);
}

bool drainUART() {
  while (Serial1.available()) {
    uint8_t b = Serial1.read();
    switch (rxState) {
      case WAIT_S1:
        if (b == START_B1) rxState = WAIT_S2;
        break;
      case WAIT_S2:
        if (b == START_B2) {
          rxState = LEN0;
          frameLen = bytesRead = 0;
        } else if (b == START_B1) rxState = WAIT_S2;
        else rxState = WAIT_S1;
        break;
      case LEN0:
        frameLen = ((uint32_t)b << 24);
        rxState = LEN1;
        break;
      case LEN1:
        frameLen |= ((uint32_t)b << 16);
        rxState = LEN2;
        break;
      case LEN2:
        frameLen |= ((uint32_t)b << 8);
        rxState = LEN3;
        break;
      case LEN3:
        frameLen |= b;
        if (frameLen == 0 || frameLen > MAX_FRAME_SIZE) resetRx();
        else rxState = PAYLOAD;
        break;
      case PAYLOAD:
        frameBuf[bytesRead++] = b;
        if (bytesRead >= frameLen) rxState = END1;
        break;
      case END1: rxState = (b == END_B1) ? END2 : WAIT_S1; break;
      case END2:
        lastFrameLen = frameLen;
        resetRx();
        if (b == END_B2) return true;
        break;
    }
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(CAM_BAUD);

  if (!BLE.begin()) {
    while (1)
      ;
  }
  BLE.setLocalName("NanoCarController");
  BLE.setAdvertisedService(carService);
  carService.addCharacteristic(commandChar);
  BLE.addService(carService);
  commandChar.writeValue((uint8_t)'s');
  BLE.advertise();

  requestFrame();
}

void loop() {
  static unsigned long t = millis();
  BLE.poll();
  if (drainUART()) {
    processFrame(frameBuf, lastFrameLen);
    requestFrame();
    t = millis();
  } else if (millis() - t > FRAME_TIMEOUT_MS) {
    requestFrame();
    t = millis();
  }
}
