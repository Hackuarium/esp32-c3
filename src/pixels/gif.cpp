#include <SPIFFS.h>
#include "./pixels.h"
#include "AnimatedGIF.h"
#include "config.h"
#ifdef MAX_LED

#include <Adafruit_NeoPixel.h>
#include "esp_spiffs.h"

File file;

// https://github.com/bitbank2/AnimatedGIF/tree/master

// First example to implement:
// https://github.com/bitbank2/AnimatedGIF/blob/master/examples/NeoPixel_Player/NeoPixelPlayer.ino

AnimatedGIF gif;
Adafruit_NeoPixel gifPixels;

void GIFDraw(GIFDRAW* pDraw);

int gifInterframeMs = 0;
int currentGif = 0;
uint8_t gifBuffer[5000] = {0};
char previousGIF[100];

void setGIF(const char* newGIF) {
  // should we change the gif ?

  if (strcmp(newGIF, previousGIF) == 0) {
    return;
  }
  strcpy(previousGIF, newGIF);

  if (currentGif) {
    gif.close();
  }
  gif.begin(GIF_PALETTE_RGB888);
  file = SPIFFS.open(newGIF, "r");
  if (!file) {
    Serial.print("Failed to open file for reading: ");
    Serial.println(newGIF);
    return;
  }
  unsigned int file_size = file.size();
  file.readBytes((char*)gifBuffer, file_size);
  currentGif = gif.openFLASH(gifBuffer, file_size, GIFDraw);
}

//
// GIF decoder callback function
// called once per line as the image is decoded
//
void GIFDraw(GIFDRAW* pDraw) {
  uint8_t r, g, b, *s, *p, *pPal = (uint8_t*)pDraw->pPalette;
  int x, y = pDraw->iY + pDraw->y;

  s = pDraw->pPixels;

  int width = pDraw->iWidth;
  int height = pDraw->iHeight;

  // Apply the new gifPixels to the main image
  if (pDraw->ucHasTransparency) {
    // if transparency used
    const uint8_t ucTransparent = pDraw->ucTransparent;
    for (x = 0; x < width; x++) {
      if (s[x] == ucTransparent) {
        gifPixels.setPixelColor(getLedIndex(y, x), getBackgroundColor());
      } else {
        p = &pPal[s[x] * 3];
        gifPixels.setPixelColor(getLedIndex(y, x),
                                gifPixels.Color(p[0], p[1], p[2]));
      }
    }
  } else {
    // no transparency, just copy them all
    for (x = 0; x < width; x++) {
      p = &pPal[s[x] * 3];
      gifPixels.setPixelColor(getLedIndex(y, x),
                              gifPixels.Color(p[0], p[1], p[2]));
    }
  }
} /* GIFDraw() */

void updateGif(Adafruit_NeoPixel& pixels) {
  gifPixels = pixels;

  if (gifInterframeMs > 0) {
    gifInterframeMs -= 40;
    if (gifInterframeMs < 0) {
      gifInterframeMs = 0;
    }
    return;
  }

  if (currentGif) {
    gif.playFrame(false,
                  &gifInterframeMs);  // play a frame and pause for
  }
  if (gifInterframeMs < 100) {
    gifInterframeMs = 100;
  }
}

#endif