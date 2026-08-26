#pragma once

#include <cstdint>

struct EpdFontData {
  const void* groups = nullptr;
};

class EpdFontFamily {
 public:
  enum Style : uint8_t {
    REGULAR = 0,
    BOLD = 1,
    ITALIC = 2,
    BOLD_ITALIC = 3,
    SMALL_CAPS = 64,
  };

  const EpdFontData* getData(Style) const { return nullptr; }
};
