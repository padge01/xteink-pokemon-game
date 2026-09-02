#include <Arduino.h>

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>

#include "lib/Epub/Epub/converters/ImageDimsProbe.h"
#include "lib/Epub/Epub/converters/ImageToFramebufferDecoder.h"

namespace {

bool check(const bool condition, const char* message) {
  if (condition) return true;
  std::cerr << message << '\n';
  return false;
}

std::array<uint8_t, 24> pngHeader(const uint32_t width, const uint32_t height) {
  return {0x89,
          0x50,
          0x4E,
          0x47,
          0x0D,
          0x0A,
          0x1A,
          0x0A,
          0x00,
          0x00,
          0x00,
          0x0D,
          'I',
          'H',
          'D',
          'R',
          static_cast<uint8_t>(width >> 24),
          static_cast<uint8_t>(width >> 16),
          static_cast<uint8_t>(width >> 8),
          static_cast<uint8_t>(width),
          static_cast<uint8_t>(height >> 24),
          static_cast<uint8_t>(height >> 16),
          static_cast<uint8_t>(height >> 8),
          static_cast<uint8_t>(height)};
}

}  // namespace

int main() {
  ImageDimensions dimensions{-1, -1};
  if (!check(ImageToFramebufferDecoder::validateAndStoreDimensions(2048, 4096, dimensions, "test"),
             "An 8 MP image at the supported boundary was rejected"))
    return 1;
  if (!check(dimensions.width == 2048 && dimensions.height == 4096,
             "Valid dimensions were not stored without narrowing"))
    return 1;

  dimensions = {123, 456};
  if (!check(!ImageToFramebufferDecoder::validateAndStoreDimensions(4096, 2049, dimensions, "test"),
             "An image above 8 MP was accepted"))
    return 1;
  if (!check(dimensions.width == 123 && dimensions.height == 456, "Rejected dimensions modified the caller's output"))
    return 1;
  if (!check(!ImageToFramebufferDecoder::validateAndStoreDimensions(32768, 1, dimensions, "test"),
             "A width that cannot fit the section-cache representation was accepted"))
    return 1;
  if (!check(!ImageToFramebufferDecoder::validateAndStoreDimensions(std::numeric_limits<int64_t>::max(), 2, dimensions,
                                                                    "test"),
             "An overflowing source-area calculation was accepted"))
    return 1;
  if (!check(!ImageToFramebufferDecoder::validateAndStoreDimensions(0, 100, dimensions, "test"),
             "Zero-width dimensions were accepted"))
    return 1;

  ImageDimsProbe validProbe;
  const auto validHeader = pngHeader(2048, 4096);
  validProbe.write(validHeader.data(), validHeader.size());
  if (!check(validProbe.getDimensions(dimensions), "The streaming probe rejected a valid 8 MP PNG header")) return 1;

  ImageDimsProbe oversizedProbe;
  const auto oversizedHeader = pngHeader(4096, 2049);
  oversizedProbe.write(oversizedHeader.data(), oversizedHeader.size());
  if (!check(!oversizedProbe.getDimensions(dimensions), "The streaming probe accepted an oversized PNG header"))
    return 1;

  uint32_t lastYieldMs = 1000;
  imageDecoderTestMillis = 1249;
  imageDecoderTestDelayCalls = 0;
  ImageToFramebufferDecoder::yieldDuringDecode(lastYieldMs);
  if (!check(lastYieldMs == 1000 && imageDecoderTestDelayCalls == 0, "Decode yielded before 250 ms")) return 1;

  imageDecoderTestMillis = 1250;
  ImageToFramebufferDecoder::yieldDuringDecode(lastYieldMs);
  if (!check(lastYieldMs == 1250 && imageDecoderTestDelayCalls == 1, "Decode did not yield at 250 ms")) return 1;

  lastYieldMs = std::numeric_limits<uint32_t>::max() - 99U;
  imageDecoderTestMillis = 150;
  ImageToFramebufferDecoder::yieldDuringDecode(lastYieldMs);
  if (!check(lastYieldMs == 150 && imageDecoderTestDelayCalls == 2, "Decode yield failed across timer rollover"))
    return 1;

  return 0;
}
