#include <AsyncMqttClient.h>
#include <StringStream.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include "./taskSerial.cpp"
#include "config.h"
#include "params.h"

AsyncMqttClient mqttClient;

char broker[40];
char subscribeTopic[40];
char publishTopic[40];
char logPublishTopic[40];

void retrieveMQTTParameters() {
  getParameter("mqtt.broker", broker);
  getParameter("mqtt.subscribe", subscribeTopic);
  getParameter("mqtt.publish", publishTopic);
  getParameter("mqtt.logpublish", logPublishTopic);
}

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
  retrieveMQTTParameters();

  vTaskDelay(1000);

  if (strlen(broker) < 2 ||
      (strlen(subscribeTopic) == 0 && strlen(publishTopic) == 0)) {
    Serial.println("MQTT: No broker or topic defined defined");
    while (strlen(broker) < 2) {
      vTaskDelay(1000);
      retrieveMQTTParameters();
    }
  }

  if (strlen(subscribeTopic) != 0 || strlen(publishTopic) != 0) {
    mqttClient.setServer(broker, 41013);
    mqttClient.onConnect(onMqttConnect);
    mqttClient.onDisconnect(onMqttDisconnect);
  };

  if (strlen(subscribeTopic) != 0) {
    mqttClient.onSubscribe(onMqttSubscribe);
    mqttClient.onUnsubscribe(onMqttUnsubscribe);
    mqttClient.onMessage(onMqttMessage);
  }

  if (strlen(publishTopic) != 0) {
    //   mqttClient.onPublish(onMqttPublish);
  }

  while (true) {
    while (WiFi.status() != WL_CONNECTED) {
      vTaskDelay(1000);
    }

    while (true) {
      vTaskDelay(5 * 1000);
      if (WiFi.status() != WL_CONNECTED) {
        break;
      }
      byte counter = 0;
      while (!mqttClient.connected() && counter++ < 10) {
        Serial.println("Connecting to MQTT...");
        mqttClient.connect();
        vTaskDelay(5 * 1000);
      }
      if (mqttClient.connected()) {
        clearParameterBit(PARAM_STATUS, PARAM_STATUS_FLAG_NO_MQTT);
      } else {
        setParameterBit(PARAM_STATUS, PARAM_STATUS_FLAG_NO_MQTT);
      }
      if (strlen(logPublishTopic) == 0) {
        continue;
      }
      //   mqttMessage = "";
      //  StringStream stream((String&)mqttMessage);
      // printResult("uc", &stream);
      //    mqttClient.publish(logPublishTopic, 1, true, &mqttMessage[0]);
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
  // Serial.println("Publish acknowledged.");
  // Serial.print("  packetId: ");
  // Serial.println(packetId);
  setParameterBit(PARAM_STATUS, PARAM_STATUS_FLAG_MQTT_PUBLISHED);
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
                          5000,  // This stack size can be checked & adjusted
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
  if (true) {
    Serial.println("Message received.");
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
