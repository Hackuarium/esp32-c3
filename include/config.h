#define KIND_ROCKET 1
#define KIND_EXAMPLE 2

#define BOARD_TYPE KIND_ROCKET

#if BOARD_TYPE == KIND_ROCKET
#include "./configRocket.h"
#elif BOARD_TYPE == KIND_EXAMPLE
#include "./configExample.h"
#else
#error "Unknown board type"
#endif
