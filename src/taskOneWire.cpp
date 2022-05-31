#include "./common.h"
#include "./params.h"

#include <OneWire.h>

void TaskOneWire(void* pvParameters) {
  (void)pvParameters;

  OneWire ds(6);

  byte i;
  byte present = 0;
  byte data[9];
  byte addr[8];
  float celsius;

  while (true) {
    vTaskDelay(1000);

    if (!ds.search(addr)) {
      ds.reset_search();
      continue;
    }

    ds.reset();
    ds.select(addr);
    ds.write(0x44, 1);  // start conversion, with parasite power on at the end

    vTaskDelay(1000);  // maybe 750ms is enough, maybe not
    // we might do a ds.depower() here, but the reset will take care of it.

    ds.reset();
    ds.select(addr);
    ds.write(0xBE);  // Read Scratchpad

    for (i = 0; i < 9; i++) {  // we need 9 bytes
      data[i] = ds.read();
    }

    int16_t raw = (data[1] << 8) | data[0];

    celsius = (float)raw / 16.0;

    setParameter(PARAM_TEMPERATURE, celsius * 100);
  }
}

void taskOneWire() {
  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(TaskOneWire, "TaskOneWire",
                          1024,  // Crash if less than 1024 !!!!
                                 // This stack size can be checked & adjusted by
                                 // reading the Stack Highwater
                          NULL,
                          0,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}
