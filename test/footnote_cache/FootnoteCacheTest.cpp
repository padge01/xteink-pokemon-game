#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "Epub/Page.h"
#include "Epub/SectionCacheFormat.h"

namespace {

class MemoryFile {
 public:
  size_t write(const void* source, const size_t count) {
    if (position_ + count > bytes_.size()) {
      bytes_.resize(position_ + count);
    }
    std::memcpy(bytes_.data() + position_, source, count);
    position_ += count;
    return count;
  }

  int read(void* destination, const size_t count) {
    if (position_ + count > bytes_.size()) {
      return 0;
    }
    std::memcpy(destination, bytes_.data() + position_, count);
    position_ += count;
    return static_cast<int>(count);
  }

  bool seek(const size_t position) {
    if (position > bytes_.size()) {
      return false;
    }
    position_ = position;
    return true;
  }

  size_t size() const { return bytes_.size(); }

 private:
  std::vector<uint8_t> bytes_;
  size_t position_ = 0;
};

bool check(const bool condition, const char* message) {
  if (condition) {
    return true;
  }
  std::cerr << message << '\n';
  return false;
}

}  // namespace

int main() {
  std::string target = "Text/chapter.xhtml#";
  target.append(255 - target.size(), 'x');

  Page page;
  page.addFootnote("42", target.c_str(), 7);
  if (!check(page.footnotes.size() == 1, "Page did not retain the footnote")) return 1;
  if (!check(std::string(page.footnotes.front().href) == target, "Page truncated the 255-byte target")) return 1;

  MemoryFile file;
  if (!check(footnote_cache::writeEntry(file, page.footnotes.front()), "Footnote serialization failed")) return 1;
  if (!check(file.size() == 289, "Footnote record is not 289 bytes")) return 1;
  if (!check(file.seek(0), "Could not rewind the memory file")) return 1;

  FootnoteEntry restored;
  if (!check(footnote_cache::readEntry(file, restored), "Footnote deserialization failed")) return 1;

  Page reloaded;
  reloaded.footnotes.push_back(restored);
  const auto match = std::find_if(reloaded.footnotes.begin(), reloaded.footnotes.end(),
                                  [](const FootnoteEntry& entry) { return entry.linkId == 7; });
  if (!check(match != reloaded.footnotes.end(), "Reloaded link ID did not resolve")) return 1;
  if (!check(std::string(match->href) == target, "Reloaded link resolved to a different target")) return 1;

  if (!check(!section_cache::isSupportedVersion(62), "Legacy full cache version 62 was accepted")) return 1;
  if (!check(!section_cache::isSupportedVersion(0xF7), "Legacy partial cache version 0xF7 was accepted")) return 1;
  if (!check(section_cache::isSupportedVersion(63), "Current full cache version 63 was rejected")) return 1;
  if (!check(section_cache::isSupportedVersion(0xF6), "Current partial cache version 0xF6 was rejected")) return 1;
  if (!check(!section_cache::isPartialVersion(63), "Full cache version was classified as partial")) return 1;
  if (!check(section_cache::isPartialVersion(0xF6), "Partial cache version was classified as full")) return 1;

  return 0;
}
