
#include <HTTPClient.h>
#include <WiFi.h>

static float_t forecast[36] = {0};
static float_t currentWeather[2] = {NAN}; // temperature + humidity
static char sunrise[6] = {};
static char sunset[6] = {};
static int16_t sunriseMin = 0;
static int16_t sunsetMin = 0;

HTTPClient http;

int16_t getMinutes(char* text);

/*
  Update the weather forecast
*/

void TaskForecast(void* pvParameters) {
  while (true) {
    while (WiFi.status() != WL_CONNECTED) {
      vTaskDelay(5000);
    }

    http.begin("http://weather-proxy.cheminfo.org/forecast24");

    // Send HTTP GET request
    int httpResponseCode = http.GET();

    if (httpResponseCode == 200) {
      String response = http.getString();
      currentWeather[2] = {NAN};
      // we will parse the data
      char* token = strtok((char*)response.c_str(), ",");
      uint8_t position = 0;
      while (token != NULL) {
        if (position == 37) {
          position++;
          for (int i = 0; i < 5; i++) {
            sunset[i] = token[i];
          }
          sunsetMin = getMinutes(sunset);
        } else if (position == 36) {
          position++;
          for (int i = 0; i < 5; i++) {
            sunrise[i] = token[i];
          }
          sunriseMin = getMinutes(sunrise);
        } else if (position < 36) {
          forecast[position++] = atof(token);
        } else if (position > 37 && position < 40) {
          currentWeather[position-38] = atof(token);
          position++;
        }
        token = strtok(NULL, ",");
      }
    } else {
      Serial.println("Failing to retrieve weather info");
    }
    vTaskDelay(5 * 60 * 1000);
  }
}

float_t* getForecast() {
  return forecast;
}

float_t* getCurrentWeather() {
  return currentWeather;
}

char* getSunrise() {
  return sunrise;
}

int16_t getSunriseInMin() {
  return sunriseMin;
}

int16_t getSunsetInMin() {
  return sunsetMin;
}

char* getSunset() {
  return sunset;
}

void taskForecast() {
  // Now set up two tasks to rntpdun independently.
  xTaskCreatePinnedToCore(TaskForecast, "TaskForecast",
                          10000,  // This stack size can be checked & adjusted
                                  // by reading the Stack Highwater
                          NULL,
                          0,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}

int16_t getMinutes(char* text) {
  int a = 0;
  int b = 0;
  sscanf(text, "%i:%i", &a, &b);
  return a * 60 + b;
}
