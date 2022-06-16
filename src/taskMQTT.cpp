#include <AsyncMqttClient.h>
#include <StringStream.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include "./common.h"
#include "./params.h"
#include "./taskSerial.cpp"
#include "driver/adc.h"                    // For deep sleep
#include <esp_wifi.h>
#include <esp_bt.h>

int32_t LOG_INTERVAL_DURATION_IN_SECS = 300; // The interval for measurements in seconds. Sleep will occur once MQTT has sent values and
// sleep duration in seconds will be adjusted to LOG_INTERVAL_DURATION_IN_SECS - millis()/1000 

AsyncMqttClient mqttClient;

char broker[40];
char subscribeTopic[40];
char publishTopic[40];
char logPublishTopic[40];
String mqttMessage;

void sendCommandResult(char* command);
void onMqttPublish(uint16_t packetId);
void onMqttSubscribe(uint16_t packetId, uint8_t qos);
void onMqttUnsubscribe(uint16_t packetId);
void onMqttConnect(bool sessionPresent);
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason);
void onMqttMessage(char* subscribeTopic,
                   char* payload,
                   AsyncMqttClientMessageProperties properties,
                   size_t len,
                   size_t index,
                   size_t total);

void TaskMQTT(void* pvParameters) {
  getParameter("mqtt.broker", broker);
  getParameter("mqtt.subscribe", subscribeTopic);
  getParameter("mqtt.publish", publishTopic);
  getParameter("mqtt.logpublish", logPublishTopic);

  vTaskDelay(1000);

  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(5000);
  }

  bool mqttAvailable = false;

  if (strlen(subscribeTopic) != 0 || strlen(publishTopic) != 0) {
    mqttClient.setServer(broker, 1883);
    mqttClient.onConnect(onMqttConnect);
    mqttClient.onDisconnect(onMqttDisconnect);
  };

  if (strlen(subscribeTopic) != 0) {
    mqttClient.onSubscribe(onMqttSubscribe);
    mqttClient.onUnsubscribe(onMqttUnsubscribe);
    mqttClient.onMessage(onMqttMessage);
  }

  if (strlen(publishTopic) != 0) {
    mqttClient.onPublish(onMqttPublish);
  }

  // Limit number of tries to connect to MQTT (to avoid battery drain if there's an issue with MQTT server)
  byte mqtt_tries = 0; 
  byte mqtt_max_tries = 4; 


  while (true) {

    while (!mqttClient.connected()) {
      Serial.println("Connecting to MQTT...");
      mqttClient.connect();
      vTaskDelay(5 * 1000);
      mqtt_tries++;
      if (mqtt_tries == mqtt_max_tries){
        Serial.println("Going to sleep now.");
        vTaskDelay(50);
        // Prepare before sleep
        vTaskDelay(50);
        btStop();
        esp_bt_controller_disable();
        WiFi.mode(WIFI_OFF);
        esp_sleep_enable_timer_wakeup(LOG_INTERVAL_DURATION_IN_SECS*1e6 - 1000*millis()); // set Deep sleep timer between measurements in uS
        esp_deep_sleep_start();  // Deep sleep here
        vTaskDelay(50);
        Serial.println("This is after deep sleep and will never be printed.");  
    }

    if (strlen(logPublishTopic) == 0) {
      continue;
    } 
    mqttMessage = "";
    StringStream stream((String&)mqttMessage);
    printResult("uc", &stream);
    mqttClient.publish(logPublishTopic, 0, true, &mqttMessage[0]);


    Serial.println("Going to sleep now.");
    vTaskDelay(50);
 // Prepare before sleep
    vTaskDelay(50);
    btStop();
    esp_bt_controller_disable();
    WiFi.mode(WIFI_OFF);
    esp_sleep_enable_timer_wakeup(LOG_INTERVAL_DURATION_IN_SECS*1e6 - 1000*millis()); // set Deep sleep timer between measurements in uS
    esp_deep_sleep_start();  // Deep sleep here
    vTaskDelay(50);
    Serial.println("This is after deep sleep and will never be printed.");
   
  }
}
}

char subcommand[100];
void sendCommandResultWithCommand(char* command) {
  if (sizeof(command) > sizeof(subcommand)) {
    return;
  }
  mqttMessage = "";
  StringStream stream((String&)mqttMessage);
  printResult(command, &stream);
  strcpy(subcommand, publishTopic);
  strcat(subcommand, "/");
  strcat(subcommand, command);
  mqttClient.publish(subcommand, 0, true, &mqttMessage[0]);
}

void onMqttPublish(uint16_t packetId) {
  Serial.println("Publish acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
  Serial.println("Subscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
  Serial.print("  qos: ");
  Serial.println(qos);
}

void taskMQTT() {
  xTaskCreatePinnedToCore(TaskMQTT, "TaskMQTT",
                          20000,  // This stack size can be checked & adjusted
                                  // by reading the Stack Highwater
                          NULL,
                          3,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
  if (strlen(subscribeTopic) != 0) {
    uint16_t packetIdSub = mqttClient.subscribe(subscribeTopic, 0);
    Serial.print("Subscribing at QoS 0, packetId: ");
    Serial.println(packetIdSub);
  }
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT.");
}

void onMqttUnsubscribe(uint16_t packetId) {
  Serial.println("Unsubscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void onMqttMessage(char* topic,
                   char* payload,
                   AsyncMqttClientMessageProperties properties,
                   size_t len,
                   size_t index,
                   size_t total) {
  if (false) {
    Serial.println("Publish received.");
    Serial.print("  topic: ");
    Serial.println(topic);
    Serial.print("  len: ");
    Serial.println(len);
    Serial.println(total);
    Serial.println(sizeof(payload));
    Serial.println(strlen(payload));
    Serial.println(sizeof(*payload));
    Serial.println(payload);
  }

  sendCommandResultWithCommand(payload);
}
