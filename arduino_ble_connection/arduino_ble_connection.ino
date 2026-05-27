#include <ArduinoBLE.h>

BLEService carService("19B10000-E8F2-537E-4F61C-D104768A1214");

BLEByteCharacteristic commandCharacteristic(
  "19B10001-E8F2-537E-4F6C-D104768A1214",
  BLERead | BLENotify
);

void setup() {
  Serial.begin(115200);
  while (!Serial);

  if (!BLE.begin()) {
    Serial.println("BLE failed!");
    while (1);
  }

  BLE.setLocalName("NanoCarController");
  BLE.setAdvertisedService(carService);

  carService.addCharacteristic(commandCharacteristic);
  BLE.addService(carService);

  commandCharacteristic.writeValue('s');

  BLE.advertise();

  Serial.println("BLE command server running...");
  Serial.println("Type:");
  Serial.println("1 2 3 4 s");
}

void loop() {
  BLEDevice central = BLE.central();

  if (central) {
    Serial.println("Pico connected!");

    while (central.connected()) {

      if (Serial.available()) {

        char command = Serial.read();

        if (command == '\n' || command == '\r') {
          continue;
        }

        if (command == '1' ||
            command == '2' ||
            command == '3' ||
            command == '4' ||
            command == 's') {

          commandCharacteristic.writeValue((byte)command);

          Serial.print("Sent command: ");
          Serial.println(command);
        }
      }
    }

    Serial.println("Pico disconnected");
  }
}