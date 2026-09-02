#pragma once

#include <cstdint>
#include <string>
#include <vector>

class SdCardFont {
 public:
  struct PrewarmCall {
    std::string text;
    uint8_t styleMask = 0;
    bool metadataOnly = false;
    bool includeKerning = false;
  };

  void clearCache() {}
  int prewarm(const char* text, const uint8_t styleMask, const bool metadataOnly, const bool includeKerning) {
    prewarmCalls.push_back({text, styleMask, metadataOnly, includeKerning});
    return 0;
  }
  bool lastPrewarmFailed() const { return false; }
  void logStats(const char*) {}
  void resetStats() {}

  std::vector<PrewarmCall> prewarmCalls;
};
