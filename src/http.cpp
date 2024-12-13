
// need to use native code:
// https://github.com/espressif/esp-idf/blob/5c1044d84d625219eafa18c24758d9f0e4006b2c/examples/protocols/esp_http_client/main/esp_http_client_example.c

#include <WiFi.h>
#include "config.h"
#include "esp_http_client.h"

#define MAX_HTTP_BUFFER 1000

esp_err_t httpEventHandler(esp_http_client_event_t* evt);

char httpBuffer[MAX_HTTP_BUFFER] = {0};
esp_http_client_handle_t httpClientHandle;
esp_err_t httpError;

static esp_http_client_config_t httpConfig = {
    .method = HTTP_METHOD_GET,
    .timeout_ms = 2000,
    .event_handler = httpEventHandler,
    .user_data = httpBuffer,
};

/*
  Update the weather forecast
*/
char* fetch(char* url) {
  httpConfig.url = url;
  // Serial.print("Fetching: ");
  // Serial.println(httpConfig.url);
  httpBuffer[1000] = {0};
  httpClientHandle = esp_http_client_init(&httpConfig);
  httpError = esp_http_client_perform(httpClientHandle);
  if (httpError == ESP_OK) {
    if (strlen(httpBuffer) == 0) {
      Serial.println("No data");
    } else if (strlen(httpBuffer) > 900) {
      Serial.println("Data too long");
    }
  } else {
    Serial.println("Failing to retrieve http info");
  }
  esp_http_client_cleanup(httpClientHandle);
  return httpBuffer;
}

esp_err_t httpEventHandler(esp_http_client_event_t* evt) {
  static int output_len;  // Stores number of bytes read
  switch (evt->event_id) {
    case HTTP_EVENT_ON_CONNECTED:
      //  Serial.println("Clear memory");
      // Serial.println(sizeof(evt->data));
      memset(&evt->data, 0, sizeof(evt->data));
      break;
    case HTTP_EVENT_ON_DATA:
      /*
       *  Check for chunked encoding is added as the URL for chunked encoding
       * used in this example returns binary data. However, event handler can
       * also be used in case chunked encoding is used.
       */
      // Serial.println("Received data");
      if (!esp_http_client_is_chunked_response(evt->client)) {
        // If user_data buffer is configured, copy the response into the
        // buffer
        if (evt->user_data) {
          if (output_len + evt->data_len < MAX_HTTP_BUFFER) {
            memcpy(evt->user_data + output_len, evt->data, evt->data_len);
          }
        } else {
          Serial.println("Need to define user_data");
          return ESP_FAIL;
        }
        output_len += evt->data_len;
      }

      break;
    case HTTP_EVENT_ON_FINISH:
      //    Serial.println("Finished");
      output_len = 0;
      break;
  }
  return ESP_OK;
}
