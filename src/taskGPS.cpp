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
    setParameterInt32(PARAM_GPS_LATITUDE, (int32_t)(gps.location.lat() * 1e6));
    setParameterInt32(PARAM_GPS_LONGITUDE, (int32_t)(gps.location.lng() * 1e6));
  }
  if (gps.altitude.isValid()) {
    setParameter(PARAM_GPS_ALTITUDE, (int16_t)gps.altitude.meters());
  }
  if (gps.satellites.isValid()) {
    setParameter(PARAM_GPS_SATELLITES, (int16_t)gps.satellites.value());
  }
  if (gps.hdop.isValid()) {
    /* hdop.value() is already hundredths; a dilution above 327 means the fix is
       worthless anyway, so saturating rather than wrapping the int16 is fine */
    uint32_t hdop = gps.hdop.value();
    setParameter(PARAM_GPS_HDOP, hdop > 32767 ? 32767 : (int16_t)hdop);
  }
  if (fixQuality.isValid()) {
    setParameter(PARAM_GPS_FIX_QUALITY, (int16_t)atoi(fixQuality.value()));
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

#ifdef PARAM_LORA_INTERVAL_SECONDS
/* (gt) is the whole tracker configuration in one command: the interval is the
   only free choice, the window is not. Setting DF, DG and DH by hand works
   just as well, but a wrong DH sends half a longitude, and the node keeps
   broadcasting it every minute without anything looking broken. */
static void processTrackerInterval(char* paramValue, Print* output) {
  if (paramValue[0] != '\0') {
    int16_t seconds = atoi(paramValue);
    if (seconds < 0) {
      output->println(F("Interval must be 0 (never) or a number of seconds"));
      return;
    }
    setAndSaveParameter(PARAM_LORA_BROADCAST_FIRST_PARAMETER,
                        PARAM_GPS_LATITUDE);
    setAndSaveParameter(PARAM_LORA_BROADCAST_NB_PARAMETERS,
                        PARAM_GPS_BLOCK_SIZE);
    setAndSaveParameter(PARAM_LORA_INTERVAL_SECONDS, seconds);
  }

  int16_t interval = getParameter(PARAM_LORA_INTERVAL_SECONDS);
  int16_t first = getParameter(PARAM_LORA_BROADCAST_FIRST_PARAMETER);
  int16_t count = getParameter(PARAM_LORA_BROADCAST_NB_PARAMETERS);
  if (interval <= 0) {
    output->println(F("Tracker off"));
  } else {
    output->print(F("Tracker every "));
    output->print(interval);
    output->println(F(" s"));
  }
  /* the window is reported whatever it holds: a node configured by hand, or by
     an older firmware, is broadcasting something else than the fix */
  output->print(F("Window: "));
  if (first >= 0 && first < MAX_PARAM) {
    output->print(numberToLabel((byte)first));
  } else {
    output->print(F("unset"));
  }
  output->print(F(" + "));
  output->print(count);
  if (first != PARAM_GPS_LATITUDE || count != PARAM_GPS_BLOCK_SIZE) {
    output->print(F(" - not the GPS block, gt"));
    output->print(interval > 0 ? interval : 60);
    output->print(F(" fixes it"));
  }
  output->println();
}
#endif

/* (gr) mirrors the sentences as they arrive. Parsed data cannot distinguish a
   module that says "no fix" from a wire that carries nothing or garbage, and
   the GSV lines carry the per-satellite SNR, which is what separates a cold
   almanac from a deaf antenna. TaskGPS does the echoing: reading the port from
   the serial task would steal bytes from the parser. */
static volatile uint32_t rawEchoUntil = 0;

void processGpsCommand(char command, char* paramValue, Print* output) {
  switch (command) {
    case 'i':
      printGpsInfo(output);
      break;
    case 'r': {
      uint32_t seconds = atoi(paramValue);
      if (seconds == 0) {
        seconds = 10;
      }
      rawEchoUntil = millis() + seconds * 1000;
      output->print(F("Echoing raw NMEA for "));
      output->print(seconds);
      output->println(F(" s"));
      break;
    }
#ifdef PARAM_LORA_INTERVAL_SECONDS
    case 't':
      processTrackerInterval(paramValue, output);
      break;
#endif
    default:
      output->println(F("(gi) gps info - all parsed data"));
      output->println(F("(gr) gps raw - echo the NMEA sentences, gr30 for 30 s"));
#ifdef PARAM_LORA_INTERVAL_SECONDS
      output->println(F("(gt) broadcast the fix every gt60 seconds, gt0 stops"));
#endif
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

  /* A full NMEA burst at 115200 is well over a kilobyte and this task drains
     the port every 20 ms, so the default 256-byte driver buffer overflows on a
     talkative module - and the loss shows up as a failed checksum rather than
     as an error. Must be set before the first begin(), the driver allocates it
     there. */
  GPSSerial.setRxBufferSize(2048);

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
      char inChar = (char)GPSSerial.read();
      gps.encode(inChar);
      if (rawEchoUntil != 0) {
        Serial.write(inChar);
      }
    }
    if (rawEchoUntil != 0 && (int32_t)(millis() - rawEchoUntil) >= 0) {
      rawEchoUntil = 0;
      Serial.println();
      Serial.println(F("[GPS] raw echo done"));
    }

    /* both flags are read every pass, never short-circuited: a receiver that is
       still searching updates the satellite count without ever producing a
       location, and that is exactly the case the central side needs to see */
    bool locationUpdated = gps.location.isUpdated();
    bool satellitesUpdated = gps.satellites.isUpdated();
    if (locationUpdated || satellitesUpdated) {
      publishFix();
    }

    vTaskDelay(20);
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
