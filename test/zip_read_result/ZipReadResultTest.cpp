#include <cstddef>

#include "ZipReadResult.h"

int main() {
  size_t remaining = 12;
  size_t bytesRead = 99;

  if (consumeZipReadResult(-1, remaining, bytesRead)) return 1;
  if (remaining != 12 || bytesRead != 0) return 2;

  if (!consumeZipReadResult(5, remaining, bytesRead)) return 3;
  if (remaining != 7 || bytesRead != 5) return 4;

  if (!consumeZipReadResult(0, remaining, bytesRead)) return 5;
  if (remaining != 7 || bytesRead != 0) return 6;

  if (consumeZipReadResult(8, remaining, bytesRead)) return 7;
  if (remaining != 7 || bytesRead != 0) return 8;

  return 0;
}
