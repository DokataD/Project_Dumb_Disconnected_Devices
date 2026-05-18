#include <BLE.h>

extern "C" {
#include "DEV_Config.h"
#include "MotorDriver.h"
}

BLEUUID serviceUUID("19B10000-E8F2-537E-4F6C-D104768A1214");
BLEUUID charUUID("19B10001-E8F2-537E-4F6C-D104768A1214");

BLERemoteCharacteristic* remoteChar = nullptr;

bool connected = false;

void handleCommand(char command) {
  Serial.print("Received command: ");
  Serial.println(command);

  switch (command) {
    case '1':
      Motor_All(FORWARD, 60);
      delay(1000);
      Motor_Stop_All();
      break;

    case '2':
      Motor_All(BACKWARD, 60);
      delay(1000);
      Motor_Stop_All();
      break;

    case '3':
      Motor_All(LEFT, 60);
      delay(1000);
      Motor_Stop_All();
      break;

    case '4':
      Motor_All(RIGHT, 60);
      delay(1000);
      Motor_Stop_All();
      break;

    case 's':
      Motor_Stop_All();
      break;
  }
}

bool connectToServer() {
  Serial.println("Scanning for Arduino BLE server...");

  auto devices = BLE.scan(serviceUUID, 5);

  if (devices == nullptr || devices->empty()) {
    Serial.println("No server found");
    return false;
  }

  BLEAdvertising dev = devices->front();

  Serial.println("Found Arduino BLE server");

  delay(500);

  if (!BLE.client()->connect(dev, 20)) {
    Serial.println("Connect failed");
    return false;
  }

  auto service = BLE.client()->service(serviceUUID);

  if (!service) {
    Serial.println("Service not found");
    return false;
  }

  remoteChar = service->characteristic(charUUID);

  if (!remoteChar) {
    Serial.println("Characteristic not found");
    return false;
  }

  Serial.println("Connected to Arduino!");

  return true;
}

void setup() {
  Serial.begin(115200);

  delay(2000);

  DEV_Module_Init();
  Motor_Init();
  Motor_Stop_All();

  BLE.begin("PicoCar");

  Serial.println("Pico BLE client ready");
}

void loop() {
  if (!connected) {
    connected = connectToServer();
    delay(1000);
    return;
  }

  if (remoteChar) {
    char command = remoteChar->getChar();

    if (command == '1' || command == '2' || command == '3' || command == '4' || command == 's') {
      handleCommand(command);
    }
  }

  delay(100);
}