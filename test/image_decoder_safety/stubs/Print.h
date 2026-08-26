#pragma once

#include <cstddef>
#include <cstdint>

class Print {
 public:
  virtual ~Print() = default;
  virtual size_t write(uint8_t byte) = 0;
  virtual size_t write(const uint8_t* data, const size_t len) {
    size_t written = 0;
    while (written < len && write(data[written]) == 1) written++;
    return written;
  }
};
