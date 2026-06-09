#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClient.h>

const char* SSID1 = "ACSlab";
const char* PWD1  = "lab@ACS24";

// AI Thinker pins
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

#define CAM_UART_TX  14
#define CAM_UART_RX  15
#define UART_BAUD    115200
#define REQ_BYTE     0x52

static const uint8_t FRAME_START[2] = { 0xFF, 0xAA };
static const uint8_t FRAME_END[2]   = { 0xFF, 0xBB };

WebServer server(80);
bool wifiOk = false;

bool initCamera() {
  camera_config_t config;
  config.ledc_channel=LEDC_CHANNEL_0; config.ledc_timer=LEDC_TIMER_0;
  config.pin_d0=Y2_GPIO_NUM; config.pin_d1=Y3_GPIO_NUM;
  config.pin_d2=Y4_GPIO_NUM; config.pin_d3=Y5_GPIO_NUM;
  config.pin_d4=Y6_GPIO_NUM; config.pin_d5=Y7_GPIO_NUM;
  config.pin_d6=Y8_GPIO_NUM; config.pin_d7=Y9_GPIO_NUM;
  config.pin_xclk=XCLK_GPIO_NUM; config.pin_pclk=PCLK_GPIO_NUM;
  config.pin_vsync=VSYNC_GPIO_NUM; config.pin_href=HREF_GPIO_NUM;
  config.pin_sscb_sda=SIOD_GPIO_NUM; config.pin_sscb_scl=SIOC_GPIO_NUM;
  config.pin_pwdn=PWDN_GPIO_NUM; config.pin_reset=RESET_GPIO_NUM;
  config.xclk_freq_hz=20000000;
  config.pixel_format=PIXFORMAT_JPEG;
  config.frame_size=FRAMESIZE_QQVGA;   // 160x120
  config.jpeg_quality=20;
  config.fb_count=1;
  config.fb_location=CAMERA_FB_IN_PSRAM;
  config.grab_mode=CAMERA_GRAB_LATEST;
  esp_err_t err=esp_camera_init(&config);
  if(err!=ESP_OK){ Serial.printf("Camera init failed: 0x%x\n",err); return false; }
  return true;
}

void sendFrameUART(const uint8_t *data, uint32_t len) {
  Serial2.write(FRAME_START,2);
  uint8_t lenBuf[4]={(uint8_t)(len>>24),(uint8_t)(len>>16),(uint8_t)(len>>8),(uint8_t)len};
  Serial2.write(lenBuf,4);
  Serial2.write(data,len);
  Serial2.write(FRAME_END,2);
  Serial2.flush();
}

void handleFrameRequest() {
  camera_fb_t *fb=esp_camera_fb_get();
  if(!fb){ Serial.println("fb_get failed"); return; }
  sendFrameUART(fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

const char HEADER[]  ="HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\n"
                      "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n";
const char BOUNDARY[]="\r\n--frame\r\n";
const char CTNTTYPE[]="Content-Type: image/jpeg\r\nContent-Length: ";

void handle_jpg_stream() {
  char buf[32];
  WiFiClient client=server.client();
  client.write(HEADER,strlen(HEADER));
  client.write(BOUNDARY,strlen(BOUNDARY));
  while(client.connected()){
    camera_fb_t *fb=esp_camera_fb_get();
    if(!fb){ delay(10); continue; }
    client.write(CTNTTYPE,strlen(CTNTTYPE));
    sprintf(buf,"%d\r\n\r\n",fb->len);
    client.write(buf,strlen(buf));
    client.write((char*)fb->buf,fb->len);
    client.write(BOUNDARY,strlen(BOUNDARY));
    esp_camera_fb_return(fb);
  }
}

void setup() {
  setCpuFrequencyMhz(80);
  Serial.begin(115200);
  Serial2.begin(UART_BAUD, SERIAL_8N1, CAM_UART_RX, CAM_UART_TX);
  if(!initCamera()){ Serial.println("Camera failed"); while(true) delay(1000); }
  Serial.println("Camera OK");
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID1,PWD1);
  unsigned long t=millis();
  while(WiFi.status()!=WL_CONNECTED && millis()-t<10000){ delay(200); Serial.print("."); }
  Serial.println();
  if(WiFi.status()==WL_CONNECTED){
    wifiOk=true;
    Serial.print("Stream: http://"); Serial.print(WiFi.localIP()); Serial.println("/mjpeg/1");
    server.on("/mjpeg/1",HTTP_GET,handle_jpg_stream);
    server.begin();
  } else {
    Serial.println("WiFi failed - UART only");
    WiFi.disconnect(true); WiFi.mode(WIFI_OFF);
  }
  Serial.println("Running");
}

void loop() {
  if(Serial2.available()){
    uint8_t b=Serial2.read();
    if(b==REQ_BYTE) handleFrameRequest();
  }
  if(wifiOk) server.handleClient();
}