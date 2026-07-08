#include "config.h"
#ifdef GPS_RX
#include <TinyGPSPlus.h>

#include "params.h"

HardwareSerial GPSSerial(1);
TinyGPSPlus gps;

// GGA field 6 is the fix quality (0 = no fix); the module talks GN (multi-GNSS)
TinyGPSCustom fixQuality(gps, "GNGGA", 6);

static void publishFix() {
  if (gps.location.isValid()) {
    setParameterInt32(PARAM_GPS_LATITUDE_LOW, PARAM_GPS_LATITUDE_HIGH,
                      (int32_t)(gps.location.lat() * 1e6));
    setParameterInt32(PARAM_GPS_LONGITUDE_LOW, PARAM_GPS_LONGITUDE_HIGH,
                      (int32_t)(gps.location.lng() * 1e6));
  }
  if (gps.altitude.isValid()) {
    setParameter(PARAM_GPS_ALTITUDE, (int16_t)gps.altitude.meters());
  }
  if (gps.satellites.isValid()) {
    setParameter(PARAM_GPS_SATELLITES, (int16_t)gps.satellites.value());
  }
}

static void printGpsInfo(Print* output) {
  output->println(F("=== GPS ==="));

  output->print(F("Fix quality: "));
  output->println(fixQuality.isValid() ? fixQuality.value() : "n/a");

  output->print(F("Satellites: "));
  if (gps.satellites.isValid()) {
    output->println(gps.satellites.value());
  } else {
    output->println(F("n/a"));
  }

  output->print(F("HDOP: "));
  if (gps.hdop.isValid()) {
    output->println(gps.hdop.hdop());
  } else {
    output->println(F("n/a"));
  }

  output->print(F("Location: "));
  if (gps.location.isValid()) {
    output->print(gps.location.lat(), 6);
    output->print(F(", "));
    output->print(gps.location.lng(), 6);
    output->print(F(" (age "));
    output->print(gps.location.age());
    output->println(F(" ms)"));
  } else {
    output->println(F("invalid"));
  }

  output->print(F("Altitude: "));
  if (gps.altitude.isValid()) {
    output->print(gps.altitude.meters());
    output->println(F(" m"));
  } else {
    output->println(F("n/a"));
  }

  output->print(F("Speed: "));
  if (gps.speed.isValid()) {
    output->print(gps.speed.kmph());
    output->println(F(" km/h"));
  } else {
    output->println(F("n/a"));
  }

  output->print(F("Course: "));
  if (gps.course.isValid()) {
    output->print(gps.course.deg());
    output->println(F(" deg"));
  } else {
    output->println(F("n/a"));
  }

  output->print(F("Date (UTC): "));
  if (gps.date.isValid()) {
    output->printf("%04u-%02u-%02u\n", gps.date.year(), gps.date.month(),
                   gps.date.day());
  } else {
    output->println(F("invalid"));
  }

  output->print(F("Time (UTC): "));
  if (gps.time.isValid()) {
    output->printf("%02u:%02u:%02u\n", gps.time.hour(), gps.time.minute(),
                   gps.time.second());
  } else {
    output->println(F("invalid"));
  }

  output->print(F("Chars processed: "));
  output->println(gps.charsProcessed());
  output->print(F("Sentences with fix: "));
  output->println(gps.sentencesWithFix());
  output->print(F("Failed checksum: "));
  output->println(gps.failedChecksum());
}

void processGpsCommand(char command, char* paramValue, Print* output) {
  (void)paramValue;
  switch (command) {
    case 'i':
      printGpsInfo(output);
      break;
    default:
      output->println(F("(gi) gps info - all parsed data"));
      break;
  }
}

// FPV GPS modules ship at different default baud rates; probe the usual ones
// for a real NMEA sentence ("$G...") so wiring the module just works.
static const uint32_t candidateBauds[] = {115200, 9600, 38400, 57600, 4800};

static uint32_t detectGpsBaud() {
  for (uint8_t i = 0; i < sizeof(candidateBauds) / sizeof(candidateBauds[0]);
       i++) {
    uint32_t baud = candidateBauds[i];
    Serial.print(F("[GPS] trying "));
    Serial.print(baud);
    Serial.println(F(" baud..."));
    GPSSerial.begin(baud, SERIAL_8N1, GPS_RX, -1);

    uint32_t start = millis();
    char previous = 0;
    while (millis() - start < 1500) {
      while (GPSSerial.available()) {
        char inChar = (char)GPSSerial.read();
        if (previous == '$' && inChar == 'G') {
          Serial.print(F("[GPS] detected baud "));
          Serial.println(baud);
          return baud;
        }
        previous = inChar;
      }
      vTaskDelay(10);
    }
    GPSSerial.end();
  }
  Serial.print(F("[GPS] no NMEA detected, defaulting to "));
  Serial.println(GPS_BAUD);
  return GPS_BAUD;
}

void TaskGPS(void* pvParameters) {
  (void)pvParameters;

  // Give the USB CDC serial time to enumerate after boot, otherwise the first
  // messages are printed before the monitor reconnects and are lost.
  vTaskDelay(2000);
  Serial.println();
  Serial.println(F("=== GPS task starting ==="));

  uint32_t baud = detectGpsBaud();

  // RX only: read NMEA sentences coming from the GPS module on GPS_RX
  GPSSerial.begin(baud, SERIAL_8N1, GPS_RX, -1);
  Serial.print(F("GPS task started on pin "));
  Serial.print(GPS_RX);
  Serial.print(F(" at "));
  Serial.print(baud);
  Serial.println(F(" baud. Use 'gi' to read the parsed data."));

  while (true) {
    while (GPSSerial.available()) {
      gps.encode((char)GPSSerial.read());
    }

    if (gps.location.isUpdated()) {
      publishFix();
    }

    vTaskDelay(100);
  }
}

void taskGPS() {
  xTaskCreatePinnedToCore(TaskGPS, "TaskGPS",
                          4096,  // This stack size can be checked & adjusted
                                 // by reading the Stack Highwater
                          NULL,
                          3,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}
#endif
