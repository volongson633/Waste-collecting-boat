#include "vision_service.h"
#include "app_config.h"
#include <cstring>
#include <cmath>

#if BOAT_VISION_BACKEND == 2
#if !__has_include("ei_model.h")
#error "Export your Edge Impulse library to lib/TrashModel and create include/ei_model.h; see doc/EDGE_IMPULSE.md"
#else
#include "ei_model.h"
#if EI_CLASSIFIER_OBJECT_DETECTION != 1
#error "An object detection model is required, not image classification"
#endif
namespace {
const uint8_t* inputImage;
unsigned sourceWidth, sourceHeight;
// Center crop to model aspect ratio, then nearest-neighbour sample. All target
// coordinates are mapped back into the ORIGINAL camera frame below.
unsigned cropX, cropY, cropWidth, cropHeight;
int getSignalData(size_t offset, size_t length, float* output) {
    if (offset + length > EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT) return -1;
    for (size_t i = 0; i < length; ++i) {
        const size_t index = offset + i;
        const unsigned x = cropX + (index % EI_CLASSIFIER_INPUT_WIDTH) * cropWidth / EI_CLASSIFIER_INPUT_WIDTH;
        const unsigned y = cropY + (index / EI_CLASSIFIER_INPUT_WIDTH) * cropHeight / EI_CLASSIFIER_INPUT_HEIGHT;
        const uint8_t* pixel = inputImage + (y * sourceWidth + x) * 3;
        output[i] = static_cast<float>((uint32_t(pixel[2]) << 16) | (uint32_t(pixel[1]) << 8) | pixel[0]);
    }
    return 0;
}
}
#endif
#endif

namespace vision {
const char* backendName() {
#if BOAT_VISION_BACKEND == 1
    return "red_marker_demo";
#elif BOAT_VISION_BACKEND == 2
    return "edge_impulse";
#else
    return "none";
#endif
}
bool available() { return BOAT_VISION_BACKEND != 0; }
bool detect(const uint8_t* bgr, unsigned width, unsigned height, boat::Detection& d) {
    d.found = false;
    d.confidence = 0;
    if (!bgr || !width || !height) return false;
#if BOAT_VISION_BACKEND == 1
    // Demo only: centroid of red pixels in the lower 85% of the frame.
    // Multiple red objects merge; this is NOT a trained trash detector.
    uint32_t count = 0, sumX = 0, sumY = 0;
    for (unsigned y = height * 15 / 100; y < height; y += 2) {
        for (unsigned x = 0; x < width; x += 2) {
            const uint8_t* p = bgr + (y * width + x) * 3;
            if (p[2] >= 100 && p[2] > p[1] * 1.5f && p[2] > p[0] * 1.5f) {
                ++count; sumX += x; sumY += y;
            }
        }
    }
    if (count >= 40) {
        d.found = true;
        d.x = float(sumX) / count / width;
        d.y = float(sumY) / count / height;
        d.confidence = 1.0f; // Threshold passed; not a statistical ML probability.
    }
    return true;
#elif BOAT_VISION_BACKEND == 2 && __has_include("ei_model.h")
    inputImage = bgr; sourceWidth = width; sourceHeight = height;
    cropWidth = width; cropHeight = height;
    if (width * EI_CLASSIFIER_INPUT_HEIGHT > height * EI_CLASSIFIER_INPUT_WIDTH)
        cropWidth = height * EI_CLASSIFIER_INPUT_WIDTH / EI_CLASSIFIER_INPUT_HEIGHT;
    else cropHeight = width * EI_CLASSIFIER_INPUT_HEIGHT / EI_CLASSIFIER_INPUT_WIDTH;
    cropX = (width - cropWidth) / 2; cropY = (height - cropHeight) / 2;
    ei::signal_t signal;
    signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
    signal.get_data = getSignalData;
    ei_impulse_result_t result = {};
    if (run_classifier(&signal, &result, false) != EI_IMPULSE_OK) return false;
    for (size_t i = 0; i < result.bounding_boxes_count; ++i) {
        const auto& box = result.bounding_boxes[i];
        if (!box.label || std::strcmp(box.label, config::targetLabel) != 0 ||
            !std::isfinite(box.value) || box.value < 0.7f || box.value <= d.confidence) continue;
        const float centerX = box.x + box.width * 0.5f;
        const float centerY = box.y + box.height * 0.5f;
        if (centerX < 0 || centerX > EI_CLASSIFIER_INPUT_WIDTH || centerY < 0 || centerY > EI_CLASSIFIER_INPUT_HEIGHT) continue;
        d.x = (cropX + centerX * cropWidth / EI_CLASSIFIER_INPUT_WIDTH) / width;
        d.y = (cropY + centerY * cropHeight / EI_CLASSIFIER_INPUT_HEIGHT) / height;
        d.confidence = box.value;
        d.found = true;
    }
    return true;
#else
    return false;
#endif
}
}
