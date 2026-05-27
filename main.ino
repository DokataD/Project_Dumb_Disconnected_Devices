#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include "portal_page.h"

#include "esp_camera.h"
#include "esp_timer.h"
#include "img_converters.h"
#include "HardwareSerial.h"
#include "fb_gfx.h"
#include "Arduino.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

const char* ssid = "Gamer-car";
const char* password = "stupidstupidstupid";
const int ssid_hidden = 1;
const int max_connection = 1;
const int channel = 1;

const byte DNS_PORT = 53;
DNSServer dnsServer;
AsyncWebServer server(80);

HardwareSerial uart1(2);
String uartLog = "";

const int output26 = 4;

// LED flash and photo variables for TREASURE detection
String treasurePhoto = "";
bool treasurePhotoAvailable = false;

// Camera streaming variables
bool cameraInitialized = false;
unsigned long lastCaptureTime = 0;
const unsigned long captureInterval = 100; // 100ms = ~10 FPS
String base64Image = "";
bool newImageAvailable = false;

// Camera pin definitions for ESP32-CAM
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

// Forward declarations
void addToLog(String message);
bool initializeCamera();
void captureAndEncodeImage();
String encodeBase64(uint8_t* data, size_t length);
void handleTreasureDetection();
void capturePhotoWithFlash();

// Helper function to add entries to log
void addToLog(String message) {
  String logEntry = String(millis()) + "ms: " + message + "\n";
  uartLog += logEntry;
  Serial.println("Added to log: " + message);
  
  // Keep log size manageable (limit to ~2000 characters)
  if (uartLog.length() > 2000) {
    uartLog = uartLog.substring(uartLog.length() - 1500);
  }
}

// Handle TREASURE detection - flash LED once and save photo
void handleTreasureDetection() {
  Serial.println("TREASURE detected - flashing LED and capturing photo!");
  addToLog("TREASURE detected - LED flash and photo capture");
  
  // Flash LED once (on for 500ms, then off)
  digitalWrite(output26, HIGH);
  delay(500);
  digitalWrite(output26, LOW);
  
  // Capture and save a special treasure photo
  if (cameraInitialized) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (fb && fb->len > 0) {
      treasurePhoto = encodeBase64(fb->buf, fb->len);
      treasurePhotoAvailable = true;
      esp_camera_fb_return(fb);
      Serial.println("Treasure photo captured and saved!");
      addToLog("Treasure photo captured successfully");
    } else {
      Serial.println("Failed to capture treasure photo");
      addToLog("Failed to capture treasure photo");
      if (fb) esp_camera_fb_return(fb);
    }
  } else {
    Serial.println("Camera not initialized - cannot capture treasure photo");
    addToLog("Camera not initialized for treasure photo");
  }
}

// Capture photo with flash - used for both manual and treasure photos
void capturePhotoWithFlash() {
  // Flash LED once (on for 500ms, then off)
  digitalWrite(output26, HIGH);
  delay(500);
  digitalWrite(output26, LOW);
  
  // Capture and save photo
  if (cameraInitialized) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (fb && fb->len > 0) {
      treasurePhoto = encodeBase64(fb->buf, fb->len);
      treasurePhotoAvailable = true;
      esp_camera_fb_return(fb);
      Serial.println("Photo captured with flash and saved!");
      addToLog("Photo captured with flash successfully");
    } else {
      Serial.println("Failed to capture photo");
      addToLog("Failed to capture photo");
      if (fb) esp_camera_fb_return(fb);
    }
  } else {
    Serial.println("Camera not initialized - cannot capture photo");
    addToLog("Camera not initialized for photo capture");
  }
}

// Base64 encoding function
String encodeBase64(uint8_t* data, size_t length) {
  const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  String result = "";
  
  for (size_t i = 0; i < length; i += 3) {
    uint32_t value = 0;
    int padding = 0;
    
    // First byte
    value |= (data[i] << 16);
    
    // Second byte
    if (i + 1 < length) {
      value |= (data[i + 1] << 8);
    } else {
      padding++;
    }
    
    // Third byte
    if (i + 2 < length) {
      value |= data[i + 2];
    } else {
      padding++;
    }
    
    // Convert to base64
    result += chars[(value >> 18) & 0x3F];
    result += chars[(value >> 12) & 0x3F];
    result += (padding < 2) ? chars[(value >> 6) & 0x3F] : '=';
    result += (padding < 1) ? chars[value & 0x3F] : '=';
  }
  
  return result;
}

// Initialize camera
bool initializeCamera() {
  Serial.println("Initializing camera...");
  
  // Deinitialize camera if it was previously initialized
  esp_camera_deinit();
  delay(100);
  
  // Add more aggressive power cycling and delays
  Serial.println("Power cycling camera module...");
  
  // Power down first
  pinMode(PWDN_GPIO_NUM, OUTPUT);
  digitalWrite(PWDN_GPIO_NUM, HIGH);  // Power down
  delay(500);  // Longer delay
  
  // Reset pin setup
  if (RESET_GPIO_NUM != -1) {
    pinMode(RESET_GPIO_NUM, OUTPUT);
    digitalWrite(RESET_GPIO_NUM, LOW);   // Reset
    delay(100);
    digitalWrite(RESET_GPIO_NUM, HIGH);  // Release reset
    delay(100);
  }
  
  // Power up
  digitalWrite(PWDN_GPIO_NUM, LOW);   // Power up
  delay(500);  // Longer delay for camera to stabilize
  
  // Camera configuration with fixed values
  camera_config_t config;
  
  // Zero out the config first
  memset(&config, 0, sizeof(camera_config_t));
  
  // Set only the necessary fields with known good values
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // Critical: Set proper XCLK frequency (20MHz is standard for OV2640)
  config.xclk_freq_hz = 20000000;  // 20MHz - this was the corrupted value!
  
  // Start with the most conservative settings
  config.frame_size = FRAMESIZE_QVGA;    // 320x240
  config.jpeg_quality = 12;              // 10-63, lower means higher quality
  config.fb_count = 1;                   // Single buffer to start
  
  if (psramFound()) {
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    Serial.println("Using PSRAM for frame buffer");
  } else {
    config.fb_location = CAMERA_FB_IN_DRAM;
    Serial.println("Using DRAM for frame buffer");
  }
  
  Serial.println("Camera config: QVGA, quality 12, 1 frame buffer, XCLK=20MHz");
  Serial.println("Attempting camera initialization...");

  // Single initialization attempt with proper config
  esp_err_t err = esp_camera_init(&config);
  
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    if (err == ESP_ERR_NOT_FOUND) {
      Serial.println("Camera not found - check physical connections!");
    } else if (err == ESP_ERR_NOT_SUPPORTED) {
      Serial.println("Camera configuration not supported!");
    } else if (err == ESP_ERR_NO_MEM) {
      Serial.println("Not enough memory for camera!");
    }
    addToLog("Camera initialization failed with error: " + String(err, HEX));
    return false;
  }

  Serial.println("Camera initialized successfully!");

  // Get camera sensor and test it
  sensor_t * s = esp_camera_sensor_get();
  if (s == NULL) {
    Serial.println("Failed to get camera sensor");
    addToLog("Failed to get camera sensor");
    return false;
  }
  
  Serial.println("Camera sensor obtained successfully");
  Serial.printf("Sensor PID: 0x%02X VER: 0x%02X MIDL: 0x%02X MIDH: 0x%02X\n", 
                s->id.PID, s->id.VER, s->id.MIDL, s->id.MIDH);
  
  // Minimal sensor settings for initial test
  s->set_brightness(s, 0);     // -2 to 2
  s->set_contrast(s, 0);       // -2 to 2
  s->set_saturation(s, 2);     // -2 to 2
  s->set_whitebal(s, 1);       // enable auto white balance
  s->set_awb_gain(s, 1);       // enable AWB gain
  s->set_exposure_ctrl(s, 1);  // enable auto exposure
  s->set_gain_ctrl(s, 1);      // enable auto gain
  s->set_hmirror(s, 0);        // no horizontal mirror
  s->set_vflip(s, 0);          // no vertical flip

  Serial.println("Camera initialized and configured successfully");
  addToLog("Camera initialized successfully - " + String(s->id.PID, HEX));
  return true;
}

// Capture image and encode to base64
void captureAndEncodeImage() {
  if (!cameraInitialized) {
    Serial.println("Camera not initialized, skipping capture");
    return;
  }
  
  Serial.println("Attempting to capture frame...");
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed - fb is null");
    addToLog("Camera capture failed - fb null");
    return;
  }

  if (fb->len == 0) {
    Serial.println("Camera capture failed - frame buffer length is 0");
    esp_camera_fb_return(fb);
    return;
  }

  Serial.println("Frame captured successfully, size: " + String(fb->len) + " bytes");
  Serial.println("Frame format: " + String(fb->format));
  Serial.println("Frame width: " + String(fb->width) + ", height: " + String(fb->height));

  // Encode to base64
  Serial.println("Encoding to base64...");
  base64Image = encodeBase64(fb->buf, fb->len);
  newImageAvailable = true;
  
  Serial.println("Base64 encoding complete, length: " + String(base64Image.length()));
  
  // Return the frame buffer back to the driver for reuse
  esp_camera_fb_return(fb);
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  Serial.println("\n=== ESP32 Camera Debug Start ===");
  
  // Initialize UART
  uart1.begin(115200, SERIAL_8N1, 12, 13);
  uart1.setRxBufferSize(2048);
  uart1.setTimeout(10);

  Serial.println("UART initialized with larger buffer");
  pinMode(output26, OUTPUT);
  digitalWrite(output26, LOW);
  Serial.println("UART and GPIO initialized");

  uartLog = "System started at " + String(millis()) + "ms\n";
  uartLog += "UART initialized successfully\n";
  uartLog += "Waiting for UART data...\n";

  // Start WiFi AP
  Serial.println("Starting WiFi AP...");
  WiFi.softAP(ssid, password, channel, ssid_hidden, max_connection);
  Serial.println("AP IP address: " + WiFi.softAPIP().toString());

  // Start DNS server
  Serial.println("Starting DNS server...");
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  Serial.println("DNS server started");

  // Setup web server routes
  Serial.println("Setting up web server routes...");
  
  // Movement control routes
  server.on("/action/forward", HTTP_GET, [](AsyncWebServerRequest *request){
    uart1.println("forward");
    uart1.flush();
    Serial.println("Sent: forward");
    addToLog("CMD: forward");
    request->send(200, "text/plain", "Moving forward");
  });

  server.on("/action/backward", HTTP_GET, [](AsyncWebServerRequest *request){
    uart1.println("backward");
    uart1.flush();
    Serial.println("Sent: backward");
    addToLog("CMD: backward");
    request->send(200, "text/plain", "Moving backward");
  });

  server.on("/action/left", HTTP_GET, [](AsyncWebServerRequest *request){
    uart1.println("left");
    uart1.flush();
    Serial.println("Sent: left");
    addToLog("CMD: left");
    request->send(200, "text/plain", "Turning left");
  });

  server.on("/action/right", HTTP_GET, [](AsyncWebServerRequest *request){
    uart1.println("right");
    uart1.flush();
    Serial.println("Sent: right");
    addToLog("CMD: right");
    request->send(200, "text/plain", "Turning right");
  });

  server.on("/action/rotateleft", HTTP_GET, [](AsyncWebServerRequest *request){
    uart1.println("rotateleft");
    uart1.flush();
    Serial.println("Sent: rotateleft");
    addToLog("CMD: rotateleft");
    request->send(200, "text/plain", "Rotating left");
  });

  server.on("/action/rotateright", HTTP_GET, [](AsyncWebServerRequest *request){
    uart1.println("rotateright");
    uart1.flush();
    Serial.println("Sent: rotateright");
    addToLog("CMD: rotateright");
    request->send(200, "text/plain", "Rotating right");
  });

  server.on("/action/stop", HTTP_GET, [](AsyncWebServerRequest *request){
    uart1.println("stop");
    uart1.flush();
    Serial.println("Sent: stop");
    addToLog("CMD: stop");
    request->send(200, "text/plain", "Stopped");
  });

  server.on("/action/camleft", HTTP_GET, [](AsyncWebServerRequest *request){
    uart1.println("camleft");
    uart1.flush();
    Serial.println("Sent: camleft");
    addToLog("CMD: camleft");
    request->send(200, "text/plain", "Camera left");
  });

  server.on("/action/camright", HTTP_GET, [](AsyncWebServerRequest *request){
    uart1.println("camright");
    uart1.flush();
    Serial.println("Sent: camright");
    addToLog("CMD: camright");
    request->send(200, "text/plain", "Camera right");
  });

  // Manual photo capture endpoint
  server.on("/camera/manual", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("Manual photo capture requested");
    addToLog("Manual photo capture requested");
    
    capturePhotoWithFlash();
    
    if (treasurePhotoAvailable) {
      request->send(200, "text/plain", "Photo captured successfully with flash");
    } else {
      request->send(500, "text/plain", "Failed to capture photo");
    }
  });

  // Treasure photo endpoint - returns the saved treasure photo
  server.on("/camera/treasure", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("Treasure photo requested");
    
    if (treasurePhotoAvailable && treasurePhoto.length() > 0) {
      Serial.println("Sending treasure photo data");
      request->send(200, "text/plain", treasurePhoto);
    } else {
      Serial.println("No treasure photo available");
      request->send(404, "text/plain", "No treasure photo available");
    }
  });

  // Camera image endpoint - returns current base64 image
  server.on("/camera/image", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("Camera image requested");
    Serial.println("Camera initialized: " + String(cameraInitialized));
    Serial.println("Base64 image length: " + String(base64Image.length()));
    
    if (cameraInitialized && base64Image.length() > 0) {
      Serial.println("Sending image data");
      request->send(200, "text/plain", base64Image);
    } else {
      Serial.println("No image available - sending error");
      String errorMsg = "No image available. Camera init: " + String(cameraInitialized) + ", Image len: " + String(base64Image.length());
      request->send(404, "text/plain", errorMsg);
    }
  });

  // Camera status endpoint
  server.on("/camera/status", HTTP_GET, [](AsyncWebServerRequest *request){
    String status = "{";
    status += "\"initialized\":" + String(cameraInitialized ? "true" : "false") + ",";
    status += "\"imageAvailable\":" + String(newImageAvailable ? "true" : "false") + ",";
    status += "\"imageSize\":" + String(base64Image.length()) + ",";
    status += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
    status += "\"psramFound\":" + String(psramFound() ? "true" : "false") + ",";
    status += "\"captureInterval\":" + String(captureInterval) + ",";
    status += "\"treasurePhotoAvailable\":" + String(treasurePhotoAvailable ? "true" : "false") + ",";
    status += "\"treasurePhotoSize\":" + String(treasurePhoto.length());
    status += "}";
    request->send(200, "application/json", status);
  });

  // Camera diagnostic endpoint
  server.on("/camera/diag", HTTP_GET, [](AsyncWebServerRequest *request){
    String diag = "=== Camera Diagnostic ===\n";
    diag += "PSRAM Found: " + String(psramFound() ? "YES" : "NO") + "\n";
    diag += "Free Heap: " + String(ESP.getFreeHeap()) + " bytes\n";
    diag += "Camera Initialized: " + String(cameraInitialized ? "YES" : "NO") + "\n\n";
    
    diag += "Pin Configuration:\n";
    diag += "PWDN: " + String(PWDN_GPIO_NUM) + "\n";
    diag += "RESET: " + String(RESET_GPIO_NUM) + "\n";
    diag += "XCLK: " + String(XCLK_GPIO_NUM) + "\n";
    diag += "SIOD: " + String(SIOD_GPIO_NUM) + "\n";
    diag += "SIOC: " + String(SIOC_GPIO_NUM) + "\n";
    diag += "Y9: " + String(Y9_GPIO_NUM) + "\n";
    diag += "Y8: " + String(Y8_GPIO_NUM) + "\n";
    diag += "Y7: " + String(Y7_GPIO_NUM) + "\n";
    diag += "Y6: " + String(Y6_GPIO_NUM) + "\n";
    diag += "Y5: " + String(Y5_GPIO_NUM) + "\n";
    diag += "Y4: " + String(Y4_GPIO_NUM) + "\n";
    diag += "Y3: " + String(Y3_GPIO_NUM) + "\n";
    diag += "Y2: " + String(Y2_GPIO_NUM) + "\n";
    diag += "VSYNC: " + String(VSYNC_GPIO_NUM) + "\n";
    diag += "HREF: " + String(HREF_GPIO_NUM) + "\n";
    diag += "PCLK: " + String(PCLK_GPIO_NUM) + "\n";
    
    request->send(200, "text/plain", diag);
  });

  // Camera reinit endpoint
  server.on("/camera/reinit", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("=== Manual Camera Reinitialization ===");
    addToLog("Manual camera reinit requested");
    
    if (cameraInitialized) {
      Serial.println("Deinitializing existing camera...");
      esp_camera_deinit();
      cameraInitialized = false;
      delay(1000);
    }
    
    cameraInitialized = initializeCamera();
    String result = cameraInitialized ? "Camera reinitialization successful!" : "Camera reinitialization failed!";
    request->send(200, "text/plain", result);
  });

  // Server-Sent Events for real-time image updates
  server.on("/camera/stream", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "text/event-stream");
    response->addHeader("Cache-Control", "no-cache");
    response->addHeader("Connection", "keep-alive");
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
  });

  server.on("/logs", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", uartLog);
  });

  server.onNotFound([](AsyncWebServerRequest *request){
    request->send(200, "text/html", portalPage());
  });

  Serial.println("Web server routes configured");

  // Check PSRAM availability
  Serial.println("Checking PSRAM...");
  if(psramFound()) {
    Serial.println("PSRAM found");
  } else {
    Serial.println("No PSRAM found");
  }

  // Initialize camera
  cameraInitialized = initializeCamera();
  
  if (cameraInitialized) {
    Serial.println("Camera ready for streaming");
    addToLog("Camera ready for streaming");
  } else {
    Serial.println("Camera initialization failed - continuing without camera");
    addToLog("Camera initialization failed");
  }

  Serial.println("Starting web server...");
  server.begin();
  Serial.println("=== Setup Complete ===");
  
  addToLog("Setup completed successfully");
}

void readUART() {
  static String currentLine = "";
  static unsigned long lastFlush = 0;
  static unsigned long lastChar = 0;
  
  // Clear buffer if it gets too full to prevent overflow
  if (uart1.available() > 1800) {
    Serial.println("UART buffer near full, clearing...");
    addToLog("UART buffer overflow - clearing");
    while(uart1.available()) {
      uart1.read();
    }
    currentLine = "";
    return;
  }
  
  while (uart1.available()) {
    char c = uart1.read();
    lastChar = millis();
    
    // Handle line endings
    if (c == '\n' || c == '\r' || c == '\0') {
      if (currentLine.length() > 0) {
        Serial.println("UART RX: " + currentLine);
        addToLog("UART_RX: " + currentLine);
        
        // Check for TREASURE keyword
        if (currentLine.indexOf("TREASURE") != -1) {
          capturePhotoWithFlash();
        }
        
        currentLine = "";
      }
    } else if (c >= 32 && c <= 126) {
      currentLine += c;
      
      // Prevent extremely long lines
      if (currentLine.length() > 80) {
        Serial.println("UART RX (long): " + currentLine);
        addToLog("UART_RX (long): " + currentLine);
        
        // Check for TREASURE keyword in long lines too
        if (currentLine.indexOf("TREASURE") != -1) {
          capturePhotoWithFlash();
        }
        
        currentLine = "";
      }
    }
  }
  
  // Force flush if no new characters for 500ms and we have data
  if (currentLine.length() > 0 && (millis() - lastChar > 500)) {
    Serial.println("UART RX (timeout): " + currentLine);
    addToLog("UART_RX (timeout): " + currentLine);
    
    // Check for TREASURE keyword in timeout case
    if (currentLine.indexOf("TREASURE") != -1) {
      capturePhotoWithFlash();
    }
    
    currentLine = "";
    lastFlush = millis();
  }
}

void loop() {
  dnsServer.processNextRequest();
  readUART();
  
  // Automatic camera capture at regular intervals
  if (cameraInitialized && (millis() - lastCaptureTime >= captureInterval)) {
    static int captureCount = 0;
    captureCount++;
    Serial.println("=== Capture attempt #" + String(captureCount) + " ===");
    captureAndEncodeImage();
    lastCaptureTime = millis();
  }
  
  // Add periodic heartbeat for debugging
  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat > 15000) { // Every 15 seconds
    addToLog("System heartbeat - loop running normally");
    Serial.print("Free heap: ");
    Serial.println(ESP.getFreeHeap());
    lastHeartbeat = millis();
  }
}