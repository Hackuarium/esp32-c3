#include "config.h"
#ifdef THR_AIR_TRAFFIC
#include <stdio.h>
#include <string.h>

#include "airTraffic/airFrames.h"
#include "airTraffic/airRadio.h"
#include "airTraffic/airSchedule.h"
#include "params.h"
#ifdef THR_GPS
#include "gpsClock.h"
#endif

/* The air traffic receiver: one SX1262, a timetable, and a JSON line per frame.

   The port is the feed. Every frame heard is one line,

     {"event":"air","proto":"flarm","mhz":868.200,"rssi":-91,"ms":517,
      "ok":true,"errors":0,"hex":"..."}

   - proto is flarm, adsl, ogn or fanet;
   - ms is where in the UTC second it landed, when the second is known - the
     measurement that says whether the timetable holds, and what (C) is
     calibrated from;
   - ok is the frame's own check, absent where this side does not check it
     (OGN's parity, FANET without a CRC in its header);
   - errors counts Manchester chip pairs that were neither 01 nor 10;
   - hex is the frame after its sync word (airFrames.h says what that is per
     protocol), for the host to decode: nothing here knows what the bytes mean.

   A host keeps the lines that parse; (ti) and the other answers are plain
   text, exactly as on a mesh bridge. */

#define AIR_LINE_LENGTH 320
/* The protocols are numbered from AIR_UNKNOWN to AIR_FANET. */
#define AIR_PROTOCOL_SLOTS (AIR_FANET + 1)

static uint32_t heard[AIR_PROTOCOL_SLOTS];
static uint32_t passed[AIR_PROTOCOL_SLOTS];
/* captures the sync word matched but that were no frame at all */
static uint32_t noise = 0;
/* (tn) prints those captures instead of only counting them. Off at boot, so
   the feed a host reads is frames and nothing else unless somebody asks. */
static boolean showNoise = false;
static uint32_t framesThisMinute = 0;
static uint32_t minuteStartedMillis = 0;

static void writeDefaultsWhenUntouched() {
  if (getQualifier() == AIR_QUALIFIER) {
    return;
  }
  setAndSaveParameter(PARAM_AIR_PROTOCOLS, AIR_PROTOCOLS_DEFAULT);
  setAndSaveParameter(PARAM_AIR_TURN_MS, AIR_TURN_MS_DEFAULT);
  setAndSaveParameter(PARAM_AIR_CLOCK_OFFSET_MS, AIR_CLOCK_OFFSET_MS_DEFAULT);
  setQualifier(AIR_QUALIFIER);
}

static uint8_t protocolMask() {
  int16_t value = getParameter(PARAM_AIR_PROTOCOLS);
  if (value < 0 || value > AIR_MASK_ALL) {
    return AIR_PROTOCOLS_DEFAULT;
  }
  return (uint8_t)value;
}

/* A turn shorter than a few milliseconds would spend itself retuning. */
static uint32_t turnMillis() {
  int16_t value = getParameter(PARAM_AIR_TURN_MS);
  return value < 20 ? AIR_TURN_MS_DEFAULT : (uint32_t)value;
}

static int32_t clockOffsetMillis() {
  int16_t value = getParameter(PARAM_AIR_CLOCK_OFFSET_MS);
  if (value == ERROR_VALUE || value < -1000 || value > 1000) {
    return AIR_CLOCK_OFFSET_MS_DEFAULT;
  }
  return value;
}

/* Where in the UTC second the instant atMillis falls, when the GPS says. The
   instant may precede the second the clock names - a capture read just after
   a new sentence arrived - so the arithmetic is signed. */
static boolean utcAt(uint32_t atMillis, uint32_t* secondOfDay,
                     uint16_t* millisInSecond) {
#ifdef THR_GPS
  GpsClock clock;
  if (!gpsClockRead(&clock)) {
    return false;
  }
  uint32_t start = clock.startMillis - (clock.exact ? 0 : clockOffsetMillis());
  int32_t elapsed = (int32_t)(atMillis - start);
  int32_t seconds = elapsed >= 0 ? elapsed / 1000 : -((999 - elapsed) / 1000);
  *secondOfDay = clock.secondOfDay + (uint32_t)seconds;
  *millisInSecond = (uint16_t)(elapsed - seconds * 1000);
  return true;
#else
  (void)atMillis;
  (void)secondOfDay;
  (void)millisInSecond;
  return false;
#endif
}

static AirListen scheduledListen() {
  uint32_t now = millis();
  uint32_t second;
  uint16_t millisInSecond;
  if (utcAt(now, &second, &millisInSecond)) {
    return airListenAtUtc(protocolMask(), second, millisInSecond);
  }
  return airListenRotating(protocolMask(), now / turnMillis());
}

static size_t appendHex(char* line, size_t at, const uint8_t* bytes,
                        uint8_t length) {
  static const char digits[] = "0123456789abcdef";
  for (uint8_t i = 0; i < length && at + 2 < AIR_LINE_LENGTH; i++) {
    line[at++] = digits[bytes[i] >> 4];
    line[at++] = digits[bytes[i] & 0x0F];
  }
  line[at] = '\0';
  return at;
}

/* Built whole and written once, so a line from another task cannot land in
   the middle of it. */
static void printFrame(const AirCapture* capture, const AirFrame* frame) {
  const AirListenSetting* setting = airListenSetting(capture->listen);
  char line[AIR_LINE_LENGTH];
  size_t at = snprintf(line, sizeof(line),
                       "{\"event\":\"air\",\"proto\":\"%s\",\"mhz\":%.3f,"
                       "\"rssi\":%d",
                       airProtocolName(frame->protocol), setting->frequencyMhz,
                       capture->rssi);
  uint32_t second;
  uint16_t millisInSecond;
  if (utcAt(capture->atMillis, &second, &millisInSecond)) {
    at += snprintf(line + at, sizeof(line) - at, ",\"ms\":%u", millisInSecond);
  }
  if (frame->check >= 0) {
    at += snprintf(line + at, sizeof(line) - at, ",\"ok\":%s",
                   frame->check ? "true" : "false");
  }
  if (setting->manchester) {
    at += snprintf(line + at, sizeof(line) - at, ",\"errors\":%u",
                   frame->violations);
  }
  at += snprintf(line + at, sizeof(line) - at, ",\"hex\":\"");
  at = appendHex(line, at, frame->bytes, frame->length);
  snprintf(line + at, sizeof(line) - at, "\"}");
  Serial.println(line);
}

/* What the sync word matched on, as the radio handed it over: still Manchester
   coded, so the host can see which chips broke the pattern. The RSSI is what
   tells a transmitter nearby from the noise floor. */
static void printNoise(const AirCapture* capture) {
  const AirListenSetting* setting = airListenSetting(capture->listen);
  char line[AIR_LINE_LENGTH];
  size_t at = snprintf(line, sizeof(line),
                       "{\"event\":\"noise\",\"mhz\":%.3f,\"rssi\":%d",
                       setting->frequencyMhz, capture->rssi);
  uint32_t second;
  uint16_t millisInSecond;
  if (utcAt(capture->atMillis, &second, &millisInSecond)) {
    at += snprintf(line + at, sizeof(line) - at, ",\"ms\":%u", millisInSecond);
  }
  at += snprintf(line + at, sizeof(line) - at, ",\"raw\":\"");
  uint8_t length = capture->length > 64 ? 64 : (uint8_t)capture->length;
  at = appendHex(line, at, capture->raw, length);
  snprintf(line + at, sizeof(line) - at, "\"}");
  Serial.println(line);
}

static void handleCapture(const AirCapture* capture) {
  AirFrame frame;
  if (!airFrameParse(capture->listen, capture->raw, capture->length, &frame)) {
    noise++;
    if (showNoise) {
      printNoise(capture);
    }
    return;
  }
  if (frame.protocol == AIR_FANET && capture->loraCrcChecked) {
    frame.check = 1;
  }
  heard[frame.protocol]++;
  if (frame.check == 1) {
    passed[frame.protocol]++;
  }
  framesThisMinute++;
  printFrame(capture, &frame);
}

static void printCount(Print* output, AirProtocol protocol) {
  output->print(airProtocolName(protocol));
  output->print(' ');
  output->print(heard[protocol]);
  if (protocol == AIR_OGN) {
    output->print(F(" (parity not checked yet)"));
    return;
  }
  output->print(F(" ("));
  output->print(passed[protocol]);
  output->print(F(" passed their check)"));
}

static void printAirInfo(Print* output) {
  uint8_t mask = protocolMask();
  output->println(F("=== Air traffic ==="));
  output->print(F("Protocols (A"));
  output->print(mask);
  output->print(F("):"));
  if (mask & AIR_MASK_MBAND) {
    output->print(F(" FLARM+ADS-L"));
  }
  if (mask & AIR_MASK_OGN) {
    output->print(F(" OGN"));
  }
  if (mask & AIR_MASK_FANET) {
    output->print(F(" FANET"));
  }
  if (mask & AIR_MASK_OBAND) {
    output->print(F(" ADS-L-O-band"));
  }
  output->println();

  output->print(F("Clock: "));
#ifdef THR_GPS
  GpsClock clock;
  if (gpsClockRead(&clock)) {
    if (clock.exact) {
      output->println(F("GPS PPS - following the slots"));
    } else {
      output->print(F("GPS sentences, corrected by "));
      output->print(clockOffsetMillis());
      output->println(F(" ms (C) - following the slots"));
    }
  } else
#endif
  {
    output->print(F("none - settings take turns of "));
    output->print(turnMillis());
    output->println(F(" ms (B)"));
  }

  const AirListenSetting* setting = airListenSetting(airRadioListening());
  output->print(F("Listening: "));
  output->println(setting == NULL ? "nothing" : setting->name);

  output->print(F("Heard: "));
  printCount(output, AIR_FLARM);
  output->print(F(", "));
  printCount(output, AIR_ADSL);
  output->print(F(", "));
  printCount(output, AIR_OGN);
  output->print(F(", "));
  printCount(output, AIR_FANET);
  output->println();
  output->print(F("Sync word matched by noise: "));
  output->print(noise);
  output->print(F(", LoRa CRC failed: "));
  output->println(airRadioRefused());
  output->print(F("Slowest retune: "));
  output->print(airRadioSlowestSwitchMillis());
  output->println(F(" ms"));
}

void processAirCommand(char command, char* paramValue, Print* output) {
  (void)paramValue;
  switch (command) {
    case 'i':
      printAirInfo(output);
      break;
    case 'c':
      memset(heard, 0, sizeof(heard));
      memset(passed, 0, sizeof(passed));
      noise = 0;
      output->println(F("Counts cleared"));
      break;
    case 'n':
      showNoise = !showNoise;
      output->println(showNoise ? F("Noise captures printed")
                                : F("Noise captures counted only"));
      break;
    default:
      output->println(F("(ti) info - protocols, clock, what was heard"));
      output->println(F("(tc) clear the counts"));
      output->println(F("(tn) print the captures counted as noise, or stop"));
      printParameterHelp(
          output, PARAM_AIR_PROTOCOLS,
          F("protocols: 1 FLARM+ADS-L, 2 OGN, 4 FANET, 8 O-band"));
      printParameterHelp(output, PARAM_AIR_TURN_MS,
                         F("ms per setting when there is no GPS time"));
      printParameterHelp(output, PARAM_AIR_CLOCK_OFFSET_MS,
                         F("ms the GPS sentences arrive after their second"));
      printParameterHelp(output, PARAM_AIR_FRAMES,
                         F("frames reported in the last minute"));
      break;
  }
}

void TaskAirTraffic(void* pvParameters) {
  (void)pvParameters;
  /* the USB CDC port needs a moment after boot, or the first lines are lost */
  vTaskDelay(2000);

  writeDefaultsWhenUntouched();
  if (!airRadioBegin()) {
    Serial.println(F("Air traffic receiver: the SX1262 did not start"));
    vTaskDelete(NULL);
  }
  Serial.println(F("Air traffic receiver started. 'ti' says what it hears."));
  minuteStartedMillis = millis();

  static AirCapture capture;
  uint32_t failedSwitchMillis = 0;
  boolean switchFailed = false;
  while (true) {
    /* read before retuning: a switch drops what the chip is holding */
    if (airRadioTake(&capture)) {
      handleCapture(&capture);
    }

    /* a switch that failed is retried once a second, not once a millisecond:
       each attempt prints why it failed */
    AirListen wanted = scheduledListen();
    if (wanted != airRadioListening() &&
        (!switchFailed || millis() - failedSwitchMillis >= 1000)) {
      switchFailed = !airRadioListen(wanted);
      failedSwitchMillis = millis();
    }

    if (millis() - minuteStartedMillis >= 60000) {
      minuteStartedMillis = millis();
      setParameter(PARAM_AIR_FRAMES, framesThisMinute > 32767
                                         ? 32767
                                         : (int16_t)framesThisMinute);
      framesThisMinute = 0;
    }

    /* the slots are 400 ms wide and a FLARM frame lasts 5 ms: a millisecond
       or two of lateness at a boundary costs nothing measurable */
    vTaskDelay(1);
  }
}

void taskAirTraffic() {
  xTaskCreatePinnedToCore(TaskAirTraffic, "TaskAirTraffic",
                          6144,  // the JSON line is formatted with snprintf,
                                 // floats included, on this stack
                          NULL,
                          2,  // above the console, so a slot boundary is not
                              // late because somebody is typing
                          NULL, 1);
}
#endif
