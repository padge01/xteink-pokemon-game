#pragma once

#include <EpdFontFamily.h>

class FontDecompressor {
 public:
  void clearCache() {}
  int prewarmCache(const EpdFontData*, const char*) { return 0; }
  void logStats(const char*) {}
  void resetStats() {}
};
