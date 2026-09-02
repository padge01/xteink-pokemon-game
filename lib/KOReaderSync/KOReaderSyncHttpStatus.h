#pragma once

namespace koreader_sync {

constexpr bool isSuccessfulHttpCode(const int httpCode) noexcept { return httpCode >= 200 && httpCode < 300; }

constexpr bool isNoContentProgressCode(const int httpCode) noexcept { return httpCode == 204; }

}  // namespace koreader_sync
