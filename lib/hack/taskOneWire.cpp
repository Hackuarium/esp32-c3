#include "./common.h"
#include "./params.h"

#include <OneWire.h>

// First temperature probe on pin 6 (external temperature)
OneWire oneWire(6);
byte oneWireData[9];
byte oneWireAddress[8];

// Add second probe on pin 8 (internal temperature)
OneWire oneWire2(8);
byte oneWire2Data[9];
byte oneWire2Address[8];


void TaskOneWire(void* pvParameters) {
  (void)pvParameters;

  float celsius;
  float celsius2;

  while (true) {
    vTaskDelay(1000);
    // Probe1 code
    if (!oneWire.search(oneWireAddress)) {
      oneWire.reset_search();
      continue;
    }

    oneWire.reset();
    oneWire.select(oneWireAddress);
    oneWire.write(0x44,
                  1);  // start conversion, with parasite power on at the end

    vTaskDelay(1000);  // maybe 750ms is enough, maybe not
    // we might do a oneWire.depower() here, but the reset will take care of it.

    oneWire.reset();
    oneWire.select(oneWireAddress);
    oneWire.write(0xBE);  // Read Scratchpad

    for (byte i = 0; i < 9; i++) {  // we need 9 bytes
      oneWireData[i] = oneWire.read();
    }

    int16_t raw = (oneWireData[1] << 8) | oneWireData[0];

    celsius = (float)raw / 16.0;

    setParameter(PARAM_TEMPERATURE, celsius * 100);

    // Probe2 code
    if (!oneWire2.search(oneWire2Address)) {
      oneWire2.reset_search();
      continue;
    }

    oneWire2.reset();
    oneWire2.select(oneWire2Address);
    oneWire2.write(0x44,
                  1);  // start conversion, with parasite power on at the end

    vTaskDelay(1000);  // maybe 750ms is enough, maybe not
    // we might do a oneWire.depower() here, but the reset will take care of it.

    oneWire2.reset();
    oneWire2.select(oneWire2Address);
    oneWire2.write(0xBE);  // Read Scratchpad

    for (byte i = 0; i < 9; i++) {  // we need 9 bytes
      oneWire2Data[i] = oneWire2.read();
    }

    int16_t raw2 = (oneWire2Data[1] << 8) | oneWire2Data[0];

    celsius2 = (float)raw2 / 16.0;

    setParameter(PARAM_INT_TEMPERATURE_A, celsius2 * 100);
  }
}


void oneWireInfo(Print* output) {
  output->println(F("One wire device list"));
  oneWire.reset_search();
  while (oneWire.search(oneWireAddress)) {
    for (byte i = 0; i < 8; i++) {
      output->print(' ');
      output->print(oneWireAddress[i], HEX);
    }
    output->println("");
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
