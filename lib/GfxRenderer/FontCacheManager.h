#pragma once

#include <EpdFontFamily.h>

#include <cstdint>
#include <map>
#include <string>

class FontDecompressor;
class SdCardFont;

class FontCacheManager {
 public:
  enum class PreparationPolicy : uint8_t { Normal, DictionaryLean };

  FontCacheManager(const std::map<int, EpdFontFamily>& fontMap, const std::map<int, SdCardFont*>& sdCardFonts);

  void setFontDecompressor(FontDecompressor* d);

  void clearCache();
  bool prewarmCache(int fontId, const char* utf8Text, uint8_t styleMask = 0x0F,
                    PreparationPolicy policy = PreparationPolicy::Normal);
  void logStats(const char* label = "render");
  void resetStats();

  // Scan-mode API: called by GfxRenderer::drawText() during scan pass
  bool isScanning() const;
  void recordText(const char* text, int fontId, EpdFontFamily::Style style);

  // The FontDecompressor pointer, needed by GfxRenderer::getGlyphBitmap()
  FontDecompressor* getDecompressor() const { return fontDecompressor_; }

  // RAII scope for two-pass prewarm pattern
  class PrewarmScope {
   public:
    explicit PrewarmScope(FontCacheManager& manager, PreparationPolicy policy);
    ~PrewarmScope();
    bool endScanAndPrewarm();
    PrewarmScope(PrewarmScope&& other) noexcept;
    PrewarmScope& operator=(PrewarmScope&&) = delete;
    PrewarmScope(const PrewarmScope&) = delete;
    PrewarmScope& operator=(const PrewarmScope&) = delete;

   private:
    FontCacheManager* manager_;
    PreparationPolicy policy_;
    bool active_ = true;
  };
  PrewarmScope createPrewarmScope(PreparationPolicy policy = PreparationPolicy::Normal);

 private:
  const std::map<int, EpdFontFamily>& fontMap_;
  const std::map<int, SdCardFont*>& sdCardFonts_;
  FontDecompressor* fontDecompressor_ = nullptr;

  enum class ScanMode : uint8_t { None, Scanning };
  ScanMode scanMode_ = ScanMode::None;

  // A page can mix reader, UI, and fallback fonts. Keep a small fixed set of
  // per-font scan buffers so each participating SD font is warmed in one pass.
  // Extra font ids still render correctly through the existing glyph-miss path.
  static constexpr uint8_t MAX_SCAN_FONTS = 4;
  struct ScanEntry {
    // Font ids are signed FNV hashes, so no id value is a safe free-slot sentinel.
    bool used = false;
    int fontId = 0;
    std::string text;
    uint8_t styleMask = 0;
  };
  ScanEntry scanEntries_[MAX_SCAN_FONTS];
  void resetScanEntries();
};
