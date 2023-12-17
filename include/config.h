#define KIND_ROCKET 1
#define KIND_PIXELS 2
#define KIND_EXAMPLE 99

#define BOARD_TYPE KIND_PIXELS

#if BOARD_TYPE == KIND_ROCKET
#include "./configRocket.h"
#elif BOARD_TYPE == KIND_PIXELS
#include "./configPixels.h"
#elif BOARD_TYPE == KIND_EXAMPLE
#include "./configExample.h"
#else
#error "Unknown board type"
#endif
