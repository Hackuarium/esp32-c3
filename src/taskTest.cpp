#include <NeoPixelBus.h>
#include "config.h"
#include "params.h"

void TaskTest(void* pvParameters) {
  vTaskDelay(0);

#define colorSaturation 8

  NeoPixelBus<NeoGrbFeature, NeoWs2812xMethod> strip(32, 8);
  RgbColor red(colorSaturation, 0, 0);
  RgbColor green(0, colorSaturation, 0);
  RgbColor blue(0, 0, colorSaturation);
  RgbColor white(colorSaturation);
  RgbColor black(0);

  HslColor hslRed(red);
  HslColor hslGreen(green);
  HslColor hslBlue(blue);
  HslColor hslWhite(white);
  HslColor hslBlack(black);

  while (true) {
    for (int i = 0; i < 32; i = i + 4) {
      strip.SetPixelColor(i + 0, red);
      strip.SetPixelColor(i + 1, green);
      strip.SetPixelColor(i + 2, blue);
      strip.SetPixelColor(i + 3, white);
    }

    strip.Show();

    vTaskDelay(100);
  }
}

void taskTest() {
  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(TaskTest, "TaskTest",
                          15000,  // This stack size can be checked & adjusted
                                  // by reading the Stack Highwater
                          NULL,
                          3,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}