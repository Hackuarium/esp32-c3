#define KIND_ROCKET 1
#define KIND_PIXELS 2
#define KIND_9_OUTPUTS 3
#define KIND_HANDRAIL 4
#define KIND_LORAWAN 5
// 6 was KIND_LORA_GPS, now a GPS fix broadcast on the mesh (KIND_LORA_MESH)
#define KIND_LORA_MESH 7
#define KIND_EXAMPLE 99

#if BOARD_TYPE == KIND_ROCKET
#include "./configRocket.h"
#elif BOARD_TYPE == KIND_PIXELS
#include "./configPixels.h"
#elif BOARD_TYPE == KIND_EXAMPLE
#include "./configExample.h"
#elif BOARD_TYPE == KIND_9_OUTPUTS
#include "./config9Outputs.h"
#elif BOARD_TYPE == KIND_HANDRAIL
#include "./configHandrail.h"
#elif BOARD_TYPE == KIND_LORAWAN
#include "./configLoraWan.h"
#elif BOARD_TYPE == KIND_LORA_MESH
#include "./configLoraMesh.h"
#else
#error "Unknown board type"
#endif
