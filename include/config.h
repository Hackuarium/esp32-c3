#define KIND_ROCKET 1
#define KIND_PIXELS 2
#define KIND_9_OUTPUTS 3
#define KIND_EXAMPLE 99

#define BOARD_TYPE KIND_9_OUTPUTS

#if BOARD_TYPE == KIND_ROCKET
#include "./configRocket.h"
#elif BOARD_TYPE == KIND_PIXELS
#include "./configPixels.h"
#elif BOARD_TYPE == KIND_EXAMPLE
#include "./configExample.h"
#elif BOARD_TYPE == KIND_9_OUTPUTS
#include "./config9Outputs.h"
#else
#error "Unknown board type"
#endif
