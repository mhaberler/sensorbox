
#include "util.hpp"

uint8_t volt2percent(const float v) {
    // convert voltage and scale for CR2032
    float percent = (v - 2.2f) / 0.65f * 100.0f;
    if (percent < 0.0f) {
        return 0;
    }
    if (percent > 100.0f) {
        return 100;
    }
    return (uint8_t) percent;
}

