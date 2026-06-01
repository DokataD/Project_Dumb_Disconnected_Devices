#include <ArduinoBLE.h>

BLEService carService("19B10000-E8F2-537E-4F6C-D104768A1214");

BLEByteCharacteristic commandCharacteristic(
  "19B10001-E8F2-537E-4F6C-D104768A1214",
  BLERead | BLENotify
);

void setup() {
  Serial.begin(115200);

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

  Serial.println("Ready");
}

void loop() {
  BLE.poll();

  if (Serial.available()) {
    char command = Serial.read();

    if (command == '1' || command == '2' ||
        command == '3' || command == '4' ||
        command == 's') {

      commandCharacteristic.writeValue((byte)command);
      Serial.print("Forwarded: ");
      Serial.println(command);
    }
  }
}