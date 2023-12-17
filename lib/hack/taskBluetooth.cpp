#include <NimBLEDevice.h>
#include "config.h"

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID "a10f6088-4fc9-4d29-9d78-e4e5f8e3c59f"
#define CHARACTERISTIC_UUID "6ecb3d7d-baef-4bde-91a7-f9c179117c36"

static NimBLEUUID dataUuid(SERVICE_UUID);
static NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();

void TaskBluetooth(void* pvParameters) {
  Serial.begin(115200);
  Serial.println("Starting BLE work!");

  static uint32_t count = 0;
  NimBLEDevice::init("Beemos");

  while (true) {
    vTaskDelay(5000);
    pAdvertising->stop();
    pAdvertising->setServiceData(dataUuid,
                                 std::string((char*)&count, sizeof(count)));
    pAdvertising->start();

    //    Serial.printf("Advertising count = %d\n", count);
    count++;
  }
}

void taskBluetooth() {
  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(TaskBluetooth, "TaskBluetooth",
                          4096,  // Crash if less than 4096 !!!!
                                 // This stack size can be checked & adjusted by
                                 // reading the Stack Highwater
                          NULL,
                          0,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}
