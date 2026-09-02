#pragma once

#include <cstddef>

inline bool consumeZipReadResult(const int result, size_t& remaining, size_t& bytesRead) {
  bytesRead = 0;
  if (result < 0) return false;

  const size_t count = static_cast<size_t>(result);
  if (count > remaining) return false;

  remaining -= count;
  bytesRead = count;
  return true;
}
