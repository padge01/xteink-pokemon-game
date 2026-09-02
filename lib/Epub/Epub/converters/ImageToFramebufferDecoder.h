#pragma once
#include <cstdint>
#include <memory>
#include <string>

class GfxRenderer;

struct ImageDimensions {
  int16_t width;
  int16_t height;
};

struct RenderConfig {
  int x, y;
  int maxWidth, maxHeight;
  bool useGrayscale = true;
  bool useDithering = true;
  bool performanceMode = false;
  bool useExactDimensions = false;  // If true, use maxWidth/maxHeight as exact output size (no recalculation)
  std::string cachePath;            // If non-empty, decoder will write pixel cache to this path
};

class ImageToFramebufferDecoder {
 public:
  virtual ~ImageToFramebufferDecoder() = default;

  virtual bool decodeToFramebuffer(const std::string& imagePath, GfxRenderer& renderer, const RenderConfig& config) = 0;

  virtual bool getDimensions(const std::string& imagePath, ImageDimensions& dims) const = 0;

  virtual const char* getFormatName() const = 0;

  // Decode callbacks can run for seconds on large, valid source images. Yield
  // occasionally so the watchdog's idle task can run without changing limits.
  static void yieldDuringDecode(uint32_t& lastYieldMs);

  // Validate in the decoder's wide source type before narrowing dimensions into
  // the int16_t section-cache representation. The output is unchanged on failure.
  static bool validateAndStoreDimensions(int64_t width, int64_t height, ImageDimensions& out, const char* format);

 protected:
  // Both decoders stream instead of allocating by source area: JPEG uses scaled
  // MCU bands and PNG uses scanlines with an independent row-buffer guard. The
  // cap therefore bounds decode time; the callbacks above yield during long work.
  static constexpr int64_t MAX_SOURCE_DIMENSION = INT16_MAX;
  static constexpr int64_t MAX_SOURCE_PIXELS = 8388608;  // 8 MP (for example 2048 * 4096)

  void warnUnsupportedFeature(const std::string& feature, const std::string& imagePath);
};
