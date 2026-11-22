#pragma once
#include "boat/Controller.h"
#include <stddef.h>
#include <stdint.h>

namespace vision {
const char* backendName();
bool available();
// Input is BGR888 from the camera converter pinned by Arduino 2.0.17.
bool detect(const uint8_t* bgr, unsigned width, unsigned height, boat::Detection& detection);
}
