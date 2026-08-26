#pragma once

#include <cstdint>

inline uint32_t imageDecoderTestMillis = 0;
inline uint32_t imageDecoderTestDelayCalls = 0;

inline unsigned long millis() { return imageDecoderTestMillis; }

inline void vTaskDelay(const uint32_t ticks) {
  if (ticks == 1) imageDecoderTestDelayCalls++;
}
