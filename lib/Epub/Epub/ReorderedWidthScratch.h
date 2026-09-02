#pragma once

namespace ReorderedWidthScratch {

enum class BuildResult { Success, ReserveFailed, AppendFailed };

template <typename Scratch, typename Widths, typename Order>
constexpr BuildResult build(Scratch& scratch, const Widths& widths, const Order& visualOrder) {
  if (!scratch.reserve(visualOrder.size())) return BuildResult::ReserveFailed;

  for (const auto sourceIndex : visualOrder) {
    if (!scratch.push_back(widths[sourceIndex])) return BuildResult::AppendFailed;
  }
  return BuildResult::Success;
}

}  // namespace ReorderedWidthScratch
