#include <FastLED.h>
#include "math.h"
#include "./common.h"
#include "./params.h"
#include "./pixels.h"

CRGB flameColor;

uint8_t flameStatus = 0;
uint8_t flameCounter = 0;
uint8_t flameSubCounter = 0;

// https://cpldcpu.wordpress.com/2016/01/05/reverse-engineering-a-real-candle/

void updateFlame(CRGB pixels[]) {
  if (flameCounter>0) flameCounter--;
  if (flameSubCounter>0) flameSubCounter--;

  int16_t intensity = 255-random(0,15);
  int16_t minIntensity = intensity - intensity * 0.2;
  int16_t medianIntensity = intensity - intensity * 0.1;

  switch (flameStatus) {
    case 0: //normal intensity
      intensity = minIntensity;
      if (flameCounter==0 && random(0,(8-getParameter(PARAM_INTENSITY))*50)==0) { // how often there is a flash
        flameCounter = random(6,10);
        flameStatus = 1;
      }    
      break;
    case 1: // high intensity fire
      if (flameCounter==0) {
        flameStatus = 2;
        flameCounter = random(10,25);
      }
      break;
    case 2: // chaotic
      if (flameCounter==0) {
        flameStatus = 3;
        flameCounter = 75;
      }
      if (flameSubCounter==0) {
          if (random(3)==0) {
              flameSubCounter=random(4)+3;
          }
          intensity = minIntensity;
      } else {
        intensity = medianIntensity;
      }
      break;  
    case 3: // damping 3s, 14 oscillation
      if (flameCounter==0) {
        flameStatus = 0;
      }
      intensity = minIntensity;
      
      int16_t extraIntensity = (medianIntensity-minIntensity) * flameCounter / 75;
      
      intensity += sin( flameCounter ) * extraIntensity + extraIntensity;
      break;
  }

  flameColor = getColor(getParameter(PARAM_COLOR_MODEL), getHSVSpeedChange(), intensity);
  for (int led = 0; led < MAX_LED; led++) {
    pixels[led] = flameColor;
  }
  for (int led = 5; led<MAX_LED; led++) {
    pixels[led]=CRGB(255,0,0);
  }
}