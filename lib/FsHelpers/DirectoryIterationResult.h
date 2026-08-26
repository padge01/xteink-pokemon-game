#pragma once

#include <cstdint>

namespace FsHelpers {

enum class DirectoryIterationResult : uint8_t {
  Complete,
  AllocationFailed,
  ReadFailed,
};

constexpr DirectoryIterationResult classifyDirectoryIterationEnd(const bool allocationFailed, const bool readFailed) {
  if (allocationFailed) {
    return DirectoryIterationResult::AllocationFailed;
  }
  return readFailed ? DirectoryIterationResult::ReadFailed : DirectoryIterationResult::Complete;
}

constexpr bool directoryIterationFailed(const DirectoryIterationResult result) {
  return result != DirectoryIterationResult::Complete;
}

}  // namespace FsHelpers
