#include "./common.h"
#include "./params.h"
#include "Adafruit_Si7021.h"
#include <SPI.h>


void TaskSi7021(void* pvParameters) {
  Adafruit_Si7021 sensor = Adafruit_Si7021();
  vTaskDelay(100);
  (void)pvParameters;

  Wire.begin(3,4); // Define specific i2c pins for SDA/SCL (mandatory for ESP32-C3)
  
  while(!sensor.begin()) {                            
    Serial.println("Did not find Si7021 sensor!");
    vTaskDelay(300);     
  }

  while (true) {
    int p_temp = 100*sensor.readTemperature();


    int p_humidity = 100*sensor.readHumidity(); 

      Serial.print("Temperature = ");
      Serial.print(p_temp);
      Serial.println("°C");

      Serial.print("Humidity = ");
      Serial.print(p_humidity);
      Serial.println("%");
    
      
	    // setParameter(PARAM_TEMPERATURE, s_temp);
     
      // setParameter(PARAM_HUMIDITY, s_humidity);

    vTaskDelay(1000);
  }
}

void taskSi7021() {
  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(TaskSi7021, "TaskSi7021",
                          2048,  // This stack size can be checked & adjusted by
                                 // reading the Stack Highwater
                          NULL,
                          2,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}

