#pragma once

#include <string_view>

namespace EpubTableElementPolicy {

constexpr bool isCaption(const std::string_view name) { return name == "caption"; }

constexpr bool isStandaloneCaption(const std::string_view name, const int tableDepth) {
  return tableDepth == 1 && isCaption(name);
}

constexpr bool isTransparentCellWrapper(const std::string_view name, const int tableDepth,
                                        const bool isHeaderOrBlock) {
  return tableDepth == 1 && isHeaderOrBlock && !isCaption(name);
}

}  // namespace EpubTableElementPolicy
