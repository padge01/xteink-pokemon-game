#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#define FOOTNOTE_NUMBER_LEN 32
#define FOOTNOTE_HREF_LEN 256
inline constexpr uint16_t EPUB_MAX_FOOTNOTES_PER_PAGE = 16;

struct FootnoteEntry {
  char number[FOOTNOTE_NUMBER_LEN];
  char href[FOOTNOTE_HREF_LEN];
  // Matches this destination to the laid-out words that display the link.
  // Zero is reserved for older/non-interactive entries.
  uint8_t linkId = 0;

  FootnoteEntry() {
    number[0] = '\0';
    href[0] = '\0';
  }
};

namespace footnote_cache {

inline constexpr std::size_t SERIALIZED_ENTRY_SIZE =
    FOOTNOTE_NUMBER_LEN + FOOTNOTE_HREF_LEN + sizeof(FootnoteEntry::linkId);

inline void assign(FootnoteEntry& entry, const char* number, const char* href, const uint8_t linkId) {
  std::strncpy(entry.number, number, sizeof(entry.number) - 1);
  entry.number[sizeof(entry.number) - 1] = '\0';
  std::strncpy(entry.href, href, sizeof(entry.href) - 1);
  entry.href[sizeof(entry.href) - 1] = '\0';
  entry.linkId = linkId;
}

template <typename File>
bool writeEntry(File& file, const FootnoteEntry& entry) {
  return file.write(entry.number, sizeof(entry.number)) == sizeof(entry.number) &&
         file.write(entry.href, sizeof(entry.href)) == sizeof(entry.href) &&
         file.write(&entry.linkId, sizeof(entry.linkId)) == sizeof(entry.linkId);
}

template <typename File>
bool readEntry(File& file, FootnoteEntry& entry) {
  if (file.read(entry.number, sizeof(entry.number)) != sizeof(entry.number) ||
      file.read(entry.href, sizeof(entry.href)) != sizeof(entry.href) ||
      file.read(&entry.linkId, sizeof(entry.linkId)) != sizeof(entry.linkId)) {
    return false;
  }
  entry.number[sizeof(entry.number) - 1] = '\0';
  entry.href[sizeof(entry.href) - 1] = '\0';
  return true;
}

static_assert(SERIALIZED_ENTRY_SIZE == 289);
static_assert(sizeof(FootnoteEntry) == SERIALIZED_ENTRY_SIZE);

}  // namespace footnote_cache
