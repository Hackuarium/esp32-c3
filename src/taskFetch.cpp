
// need to use native code:
// https://github.com/espressif/esp-idf/blob/5c1044d84d625219eafa18c24758d9f0e4006b2c/examples/protocols/esp_http_client/main/esp_http_client_example.c

#include <ArduinoJson.h>
#include <WiFi.h>
#include "config.h"
#include "esp_http_client.h"
#include "forecast.h"
#include "fronius.h"

/*
  How often the LED wall's energy balance is refreshed (ms).

  Deliberately shorter than the backend's 5 s HTTP keep-alive window: within it
  the connection is reused and the TLS handshake is skipped entirely (~3 ms per
  fetch instead of ~13 ms), so this cadence costs LESS per fetch than a slower
  one, which would always reconnect from cold. It also bounds how stale the wall
  can be against a backend that refreshes its own reading every 10 s.
*/
#define ENERGY_FLOW_INTERVAL_MS 2500
/* The forecast changes by the hour; it lives on another host, so each of these
   evicts the kept-alive energy-flow connection and costs one handshake. */
#define FORECAST_INTERVAL_MS 60000
/* Loop tick — must divide the shortest interval above. */
#define FETCH_TICK_MS 500

/*
  Update the weather and the energy balance, each on its own cadence.
*/
void TaskFetch(void* pvParameters) {
  uint32_t lastEnergyFlow = 0;
  uint32_t lastForecast = 0;
  vTaskDelay(10000);
  while (true) {
    while (WiFi.status() != WL_CONNECTED) {
      vTaskDelay(5000);
    }
    // Unsigned arithmetic, so the millis() wrap at ~49 days is handled. Both
    // start at 0, which is already "due" and fires on the first pass.
    uint32_t now = millis();
#ifdef FETCH_FRONIUS
    if (now - lastEnergyFlow >= ENERGY_FLOW_INTERVAL_MS) {
      updateFronius();
      lastEnergyFlow = now;
    }
#endif
#ifdef FETCH_WEATHER
    if (now - lastForecast >= FORECAST_INTERVAL_MS) {
      updateForecast();
      lastForecast = now;
    }
#endif
    vTaskDelay(FETCH_TICK_MS);
  }
}

void taskFetch() {
  // Now set up two tasks to rntpdun independently.
  xTaskCreatePinnedToCore(TaskFetch, "TaskFetch",
                          20000,  // This stack size can be checked & adjusted
                                  // by reading the Stack Highwatee
                          NULL,
                          0,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}
