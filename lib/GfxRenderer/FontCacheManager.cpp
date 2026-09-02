#include "FontCacheManager.h"

#include <FontDecompressor.h>
#include <Logging.h>
#include <SdCardFont.h>

FontCacheManager::FontCacheManager(const std::map<int, EpdFontFamily>& fontMap,
                                   const std::map<int, SdCardFont*>& sdCardFonts)
    : fontMap_(fontMap), sdCardFonts_(sdCardFonts) {}

void FontCacheManager::setFontDecompressor(FontDecompressor* d) { fontDecompressor_ = d; }

void FontCacheManager::clearCache() {
  if (fontDecompressor_) fontDecompressor_->clearCache();
  for (auto& [id, font] : sdCardFonts_) {
    font->clearCache();
  }
}

bool FontCacheManager::prewarmCache(int fontId, const char* utf8Text, uint8_t styleMask,
                                    const PreparationPolicy policy) {
  // SD card font prewarm path: prewarm all requested styles in one call
  auto it = sdCardFonts_.find(fontId);
  if (it != sdCardFonts_.end()) {
    int missed =
        it->second->prewarm(utf8Text, styleMask, /*metadataOnly=*/false, policy != PreparationPolicy::DictionaryLean);
    if (missed > 0) {
      LOG_DBG("FCM", "prewarmCache(SD): %d glyph(s) not found (styleMask=0x%02X)", missed, styleMask);
    }
    return !it->second->lastPrewarmFailed();
  }

  // Standard compressed font prewarm path: loop over all requested styles
  if (!fontDecompressor_ || fontMap_.count(fontId) == 0) return false;

  // Reverse iteration is harmless now; the decompressor keeps one retained page slot per style.
  for (int8_t i = 3; i >= 0; i--) {
    if (!(styleMask & (1 << i))) continue;
    auto style = static_cast<EpdFontFamily::Style>(i);
    const EpdFontData* data = fontMap_.at(fontId).getData(style);
    if (!data || !data->groups) continue;
    int missed = fontDecompressor_->prewarmCache(data, utf8Text);
    if (missed > 0) {
      LOG_DBG("FCM", "prewarmCache: %d glyph(s) not cached for style %d", missed, i);
    }
  }
  return true;
}

void FontCacheManager::logStats(const char* label) {
  if (fontDecompressor_) fontDecompressor_->logStats(label);
  for (auto& [id, font] : sdCardFonts_) {
    font->logStats(label);
  }
}

void FontCacheManager::resetStats() {
  if (fontDecompressor_) fontDecompressor_->resetStats();
  for (auto& [id, font] : sdCardFonts_) {
    font->resetStats();
  }
}

bool FontCacheManager::isScanning() const { return scanMode_ == ScanMode::Scanning; }

void FontCacheManager::recordText(const char* text, int fontId, EpdFontFamily::Style style) {
  if (!text || *text == '\0') return;

  ScanEntry* entry = nullptr;
  for (auto& candidate : scanEntries_) {
    if (candidate.used && candidate.fontId == fontId) {
      entry = &candidate;
      break;
    }
    if (!candidate.used) {
      candidate.used = true;
      candidate.fontId = fontId;
      // Entry zero normally holds page text. Later entries usually hold short
      // UI strings; capacities stay allocated and converge instead of churning.
      candidate.text.reserve(&candidate == &scanEntries_[0] ? 2048 : 256);
      entry = &candidate;
      break;
    }
  }
  if (!entry) return;

  if ((style & EpdFontFamily::SMALL_CAPS) != 0) {
    for (const char* p = text; *p != '\0'; ++p) {
      entry->text.push_back((*p >= 'a' && *p <= 'z') ? static_cast<char>(*p - ('a' - 'A')) : *p);
    }
  } else {
    entry->text += text;
  }
  entry->styleMask |= static_cast<uint8_t>(1U << (static_cast<uint8_t>(style) & 0x03));
}

void FontCacheManager::resetScanEntries() {
  for (auto& entry : scanEntries_) {
    entry.used = false;
    entry.fontId = 0;
    entry.styleMask = 0;
    // Keep grown capacity so repeated page turns avoid allocate-copy-free churn.
    entry.text.clear();
  }
}

// --- PrewarmScope implementation ---

FontCacheManager::PrewarmScope::PrewarmScope(FontCacheManager& manager, const PreparationPolicy policy)
    : manager_(&manager), policy_(policy) {
  manager_->scanMode_ = ScanMode::Scanning;
  manager_->clearCache();
  manager_->resetStats();
  manager_->resetScanEntries();
}

bool FontCacheManager::PrewarmScope::endScanAndPrewarm() {
  manager_->scanMode_ = ScanMode::None;
  bool ok = true;
  for (auto& entry : manager_->scanEntries_) {
    if (!entry.used || entry.text.empty()) continue;
    if (!manager_->prewarmCache(entry.fontId, entry.text.c_str(), entry.styleMask != 0 ? entry.styleMask : 1,
                                policy_)) {
      ok = false;
    }
  }
  manager_->resetScanEntries();
  return ok;
}

FontCacheManager::PrewarmScope::~PrewarmScope() {
  if (active_) {
    endScanAndPrewarm();  // no-op if already called (scan entries are empty)
    manager_->clearCache();
  }
}

FontCacheManager::PrewarmScope::PrewarmScope(PrewarmScope&& other) noexcept
    : manager_(other.manager_), policy_(other.policy_), active_(other.active_) {
  other.active_ = false;
}

FontCacheManager::PrewarmScope FontCacheManager::createPrewarmScope(const PreparationPolicy policy) {
  return PrewarmScope(*this, policy);
}
