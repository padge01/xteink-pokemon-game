#pragma once

#include <cstdint>

inline uint32_t imageDecoderTestMillis = 0;
inline uint32_t imageDecoderTestDelayCalls = 0;

inline unsigned long millis() { return imageDecoderTestMillis; }

inline void delay(const unsigned long milliseconds) {
  if (milliseconds == 1) imageDecoderTestDelayCalls++;
}
