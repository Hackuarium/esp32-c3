#include "config.h"
#include "params.h"

#include <OneWire.h>

#ifdef THR_ONEWIRE

byte oneWirePorts[] = {THR_ONEWIRE};
byte oneWireParameters[] = {PARAM_TEMPERATURE};

OneWire* oneWires[sizeof(oneWirePorts)];

// First temperature probe on pin 6 and 8 (external temperature)
OneWire oneWire;

byte oneWireData[9];
byte oneWireAddress[8];

void TaskOneWire(void* pvParameters) {
  for (byte i = 0; i < sizeof(oneWirePorts); i++) {
    oneWires[i] = new OneWire(oneWirePorts[i]);
  }

  (void)pvParameters;

  float celsius;

  while (true) {
    vTaskDelay(1000);

    for (byte i = 0; i < sizeof(oneWirePorts); i++) {
      oneWire = *oneWires[i];
      if (!oneWire.search(oneWireAddress)) {
        oneWire.reset_search();
        Serial.println("No more OneWire devices on port ");
        continue;
      }

      oneWire.reset();
      oneWire.select(oneWireAddress);
      oneWire.write(0x44,
                    1);  // start conversion, with parasite power on at the end

      vTaskDelay(1000);  // maybe 750ms is enough, maybe not
      // we might do a oneWire.depower() here, but the reset will take care of
      // it.

      oneWire.reset();
      oneWire.select(oneWireAddress);
      oneWire.write(0xBE);  // Read Scratchpad

      for (byte i = 0; i < 9; i++) {  // we need 9 bytes
        oneWireData[i] = oneWire.read();
      }

      int16_t raw = (oneWireData[1] << 8) | oneWireData[0];

      celsius = (float)raw / 16.0;

      // Serial.print("OneWire on port ");
      // Serial.println(celsius);
      setParameter(oneWireParameters[i], celsius * 100);
    }
  }
}

void oneWireInfo(Print* output) {
  output->println(F("One wire device list"));
  for (byte i = 0; i < sizeof(oneWirePorts); i++) {
    output->print("Port ");
    output->println(oneWirePorts[i]);
    oneWire = *oneWires[i];
    oneWire.reset_search();
    while (oneWire.search(oneWireAddress)) {
      for (byte i = 0; i < 8; i++) {
        output->print(' ');
        output->print(oneWireAddress[i], HEX);
      }
      output->println("");
    }
  }
}

void taskOneWire() {
  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(TaskOneWire, "TaskOneWire", 2048,
                          // This stack size can be checked & adjusted
                          // by reading the Stack Highwater
                          NULL,
                          0,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}

#endif