#include <AsyncMqttClient.h>
#include <StringStream.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include "./common.h"
#include "./params.h"
#include "./taskSerial.cpp"

AsyncMqttClient mqttClient;

char broker[40];
char subscribeTopic[40];
char publishTopic[40];

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

  while (true) {
    vTaskDelay(10 * 1000);
    while (!mqttClient.connected()) {
      Serial.println("Connecting to MQTT...");
      mqttClient.connect();
      vTaskDelay(5 * 1000);
    }

    if (strlen(publishTopic) == 0) {
      continue;
    }

    String message;
    StringStream stream((String&)message);
    printResult("uc", &stream);
    uint16_t packetIdPub1 =
        mqttClient.publish(publishTopic, 0, true, &message[0]);
  }
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
    uint16_t packetIdSub = mqttClient.subscribe(subscribeTopic, 2);
    Serial.print("Subscribing at QoS 2, packetId: ");
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
