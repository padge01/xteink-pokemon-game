#pragma once

template <typename... Args>
inline void imageDecoderTestLog(Args&&...) {}

#define LOG_ERR(...) imageDecoderTestLog(__VA_ARGS__)
