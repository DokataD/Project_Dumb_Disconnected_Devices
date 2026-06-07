#ifndef WIFI_HANDLE_H
#define WIFI_HANDLE_H

#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClient.h>
#include "frame_handle.h"
#include "wifi_credentials.h"

const char HEADER[]   = "HTTP/1.1 200 OK\r\n"
                        "Access-Control-Allow-Origin: *\r\n"
                        "Content-Type: multipart/x-mixed-replace; "
                        "boundary=123456789000000000000987654321\r\n";
const char BOUNDARY[] = "\r\n--123456789000000000000987654321\r\n";
const char CTNTTYPE[] = "Content-Type: image/jpeg\r\nContent-Length: ";

void initWifi();

void handleJPGStream();

void handleNotFound();

void handleWifiRequest();

#endif