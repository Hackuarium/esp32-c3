
// need to use native code:
// https://github.com/espressif/esp-idf/blob/5c1044d84d625219eafa18c24758d9f0e4006b2c/examples/protocols/esp_http_client/main/esp_http_client_example.c

#include <WiFi.h>
#include "./charToFloat.h"
#include "esp_http_client.h"

static float_t forecast[36] = {0};
static float_t currentWeather[2] = {NAN};  // temperature + humidity
static char sunrise[6] = {0};
static char sunset[6] = {0};
static int16_t sunriseMin = 0;
static int16_t sunsetMin = 0;

esp_err_t _http_event_handler(esp_http_client_event_t* evt);
int16_t getMinutes(char* text);

char response[200] = {0};

esp_http_client_config_t config = {
    .url = "http://weather-proxy.cheminfo.org/forecast24",
    .username = "user",
    .password = "password",
    .auth_type = HTTP_AUTH_TYPE_BASIC,
    .method = HTTP_METHOD_GET,
    .timeout_ms = 2000,
    .event_handler =
        _http_event_handler,  // need an event handler to get the result
    .user_data = response,
};

/*
  Update the weather forecast
*/
void TaskForecast(void* pvParameters) {
  char* token;

  vTaskDelay(10000);
  while (true) {
    while (WiFi.status() != WL_CONNECTED) {
      vTaskDelay(5000);
    }

    esp_http_client_handle_t client = esp_http_client_init(&config);
    // GET
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
      currentWeather[2] = {NAN};
      // we will parse the data
      token = strtok(response, ",");
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
          forecast[position] = charToFloat(token);
          position++;
        } else if (position > 37 && position < 40) {
          currentWeather[position - 38] = charToFloat(token);
          position++;
        }
        token = strtok(NULL, ",");
      }

    } else {
      Serial.println("Failing to retrieve weather info");
    }
    esp_http_client_cleanup(client);

    vTaskDelay(10 * 1000);
  }
}

void printSunrise(Print* output) {
  output->print("Sunrise: ");
  output->println(sunrise);
}

void printSunset(Print* output) {
  output->print("Sunset: ");
  output->println(sunset);
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
                          20000,  // This stack size can be checked & adjusted
                                  // by reading the Stack Highwater
                          NULL,
                          0,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}

int16_t getMinutes(char* text) {
  int a = (text[0] - 48) * 10 + text[1] - 48;
  int b = (text[3] - 48) * 10 + text[4] - 48;
  return a * 60 + b;
}

esp_err_t _http_event_handler(esp_http_client_event_t* evt) {
  static char* output_buffer;  // Buffer to store response of http request from
                               // event handler
  static int output_len;       // Stores number of bytes read
  switch (evt->event_id) {
    case HTTP_EVENT_ON_DATA:
      /*
       *  Check for chunked encoding is added as the URL for chunked encoding
       * used in this example returns binary data. However, event handler can
       * also be used in case chunked encoding is used.
       */
      if (!esp_http_client_is_chunked_response(evt->client)) {
        // If user_data buffer is configured, copy the response into the
        // buffer
        if (evt->user_data) {
          memcpy(evt->user_data + output_len, evt->data, evt->data_len);
        } else {
          if (output_buffer == NULL) {
            output_buffer =
                (char*)malloc(esp_http_client_get_content_length(evt->client));
            output_len = 0;
            if (output_buffer == NULL) {
              return ESP_FAIL;
            }
          }
          memcpy(output_buffer + output_len, evt->data, evt->data_len);
        }
        output_len += evt->data_len;
      }

      break;
    case HTTP_EVENT_ON_FINISH:
      if (output_buffer != NULL) {
        // Response is accumulated in output_buffer. Uncomment the below
        // line to print the accumulated response ESP_LOG_BUFFER_HEX(TAG,
        // output_buffer, output_len);
        free(output_buffer);
        output_buffer = NULL;
      }
      output_len = 0;
      break;
  }
  return ESP_OK;
}
