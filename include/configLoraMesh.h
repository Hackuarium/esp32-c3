#include <Arduino.h>

#define WIRE_SDA SDA
#define WIRE_SCL SCL
#define THR_WIRE_MASTER 1

#define BMP280 0x77

#define THR_ONEWIRE 3
/* enables the (a) serial menu, implemented by taskLoraMesh.cpp on this board */
#define THR_LORA 1
/* enables the (x) command that broadcasts a block of parameters */
#define THR_LORA_MESH 1

/* GPS_RX comes from the env: defining it turns this board into a tracker that
   broadcasts its fix on the mesh, and enables the (g) serial menu */
#ifdef GPS_RX
#define THR_GPS 1
#endif

extern SemaphoreHandle_t xSemaphoreWire;

#define ANALOG_SLEEP 1000
#define ANALOG_INPUTS \
  {D0, PARAM_SOLAR_MILLI_VOLTS, 10.0}, {D1, PARAM_BATTERY_MILLI_VOLTS, 2.0}

#define MAX_PARAM 38
extern int16_t parameters[MAX_PARAM];

#define DHT22PIN 43  // this corresponds to D6

#define PARAM_TEMPERATURE 0  // A
#define PARAM_HUMIDITY 1     // B
#define PARAM_PRESSURE 2     // C

#define PARAM_BATTERY_MILLI_VOLTS 3  // D
#define PARAM_SOLAR_MILLI_VOLTS 4    // E
#define PARAM_SOLAR_MILLI_AMPERES 5  // F

/* GPS fix, written by taskGPS when the env defines GPS_RX. Latitude and
   longitude are degrees * 1e6, each an int32 spread over two adjacent slots.
   taskGPS only fills these six: sending them is the generic parameter
   broadcast's job, so a tracker is "S60 AE6 AF6", not a special frame type. */
#define PARAM_GPS_LATITUDE 6     // G and H
#define PARAM_GPS_LONGITUDE 8    // I and J
#define PARAM_GPS_ALTITUDE 10    // K - meters
#define PARAM_GPS_SATELLITES 11  // L

#define PARAM_RELATIVE_ALTITUDE 15  // P
#define PARAM_ALTITUDE_GROUND 16    // Q

#define PARAM_LORA_SLEEP_SECONDS 17  // R
/* seconds between two periodic parameter broadcasts, 0 = never */
#define PARAM_LORA_INTERVAL_SECONDS 18  // S
// 7 to 12
#define PARAM_LORA_SPREADING_FACTOR 19  // T

#define PARAM_UPTIME_H 20   // U
#define PARAM_STATUS 21     // V
#define PARAM_WIFI_MODE 23  // CS - <=0: STA, 1: AP, 2: STA 30s then AP
#define PARAM_WIFI_RSSI 24  // Y
#define PARAM_ERROR 25      // Z

#define PARAM_STATUS_FLAG_NO_WIFI 0
#define PARAM_STATUS_FLAG_NO_MQTT 1
#define PARAM_STATUS_FLAG_MQTT_PUBLISHED 8

#define PARAM_LOGGING_INTERVAL 26         // AA - positive = s, negative = ms
#define PARAM_LOGGING_NB_ENTRIES 27       // AB
#define PARAM_LOGGING_FIRST_PARAMETER 28  // AC
#define PARAM_LOGGING_NB_PARAMETERS 29    // AD
/* Which block of parameters the periodic broadcast carries, same first/count
   shape as the logging window above and as the (ac) command. Sending starts at
   PARAM_LORA_BROADCAST_FIRST_PARAMETER and covers
   PARAM_LORA_BROADCAST_NB_PARAMETERS slots. */
#define PARAM_LORA_BROADCAST_FIRST_PARAMETER 30  // AE
#define PARAM_LORA_BROADCAST_NB_PARAMETERS 31    // AF

#define PARAM_SLEEP_NORMAL_DELAY 32  // AG
#define PARAM_SLEEP_ERROR_DELAY 33   // AH

/* 0 = endpoint (originates and consumes frames), 1 = repeater (does that and
   also forwards frames that are not addressed to it), 2 = bridge (an endpoint
   whose serial port is a machine readable feed - see src/lora/loraBridge.h) */
#define PARAM_LORA_ROLE 34  // AI
/* TTL used by the frames this node originates, 0 = never relayed, max 7 */
#define PARAM_LORA_TTL 35  // AJ

/* carrier in units of 0.1 MHz, so 8683 is 868.3 MHz. The whole EU868 SRD band
   sits on 0.1 MHz boundaries, and 0.1 MHz is the finest step an int16 parameter
   can carry without overflowing */
#define PARAM_LORA_FREQUENCY 36  // AK
/* bandwidth in kHz: 250, 125 or 62 (meaning 62.5) */
#define PARAM_LORA_BANDWIDTH 37  // AL

#define LORA_ROLE_ENDPOINT 0
#define LORA_ROLE_REPEATER 1
#define LORA_ROLE_BRIDGE 2
