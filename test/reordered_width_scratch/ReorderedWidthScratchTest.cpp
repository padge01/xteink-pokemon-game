#include <array>
#include <cstddef>
#include <cstdint>

#include "lib/Epub/Epub/ReorderedWidthScratch.h"

using ReorderedWidthScratch::BuildResult;
using ReorderedWidthScratch::build;

class ControlledScratch {
 public:
  constexpr ControlledScratch(const size_t reserveLimit, const size_t appendLimit)
      : reserveLimit_(reserveLimit), appendLimit_(appendLimit) {}

  constexpr bool reserve(const size_t requested) {
    if (requested > reserveLimit_ || requested > values_.size()) return false;
    capacity_ = requested;
    return true;
  }

  constexpr bool push_back(const uint16_t value) {
    if (size_ >= capacity_ || size_ >= appendLimit_) return false;
    values_[size_++] = value;
    return true;
  }

  [[nodiscard]] constexpr size_t size() const { return size_; }
  [[nodiscard]] constexpr uint16_t operator[](const size_t index) const { return values_[index]; }

 private:
  std::array<uint16_t, 3> values_{};
  size_t reserveLimit_ = 0;
  size_t appendLimit_ = 0;
  size_t capacity_ = 0;
  size_t size_ = 0;
};

constexpr std::array<uint16_t, 3> WIDTHS{10, 20, 30};
constexpr std::array<uint16_t, 3> VISUAL_ORDER{2, 0, 1};

constexpr bool preservesVisualOrder() {
  ControlledScratch scratch(3, 3);
  return build(scratch, WIDTHS, VISUAL_ORDER) == BuildResult::Success && scratch.size() == 3 &&
         scratch[0] == 30 && scratch[1] == 10 && scratch[2] == 20;
}

constexpr bool reportsReserveFailure() {
  ControlledScratch scratch(2, 3);
  return build(scratch, WIDTHS, VISUAL_ORDER) == BuildResult::ReserveFailed && scratch.size() == 0;
}

constexpr bool reportsAppendFailure() {
  ControlledScratch scratch(3, 1);
  return build(scratch, WIDTHS, VISUAL_ORDER) == BuildResult::AppendFailed && scratch.size() == 1;
}

static_assert(preservesVisualOrder());
static_assert(reportsReserveFailure());
static_assert(reportsAppendFailure());

int main() { return 0; }
