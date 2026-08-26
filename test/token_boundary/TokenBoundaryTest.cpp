#include <cstdint>
#include <iomanip>
#include <iostream>

#include "lib/Epub/Epub/TokenBoundary.h"

namespace {

bool check(const bool condition, const char* message) {
  if (condition) return true;
  std::cerr << message << '\n';
  return false;
}

}  // namespace

int main() {
  if (!check(TokenBoundary::allowsBreak(/*continues=*/false, /*noSpaceBefore=*/false),
             "Ordinary word gap was not breakable"))
    return 1;
  if (!check(TokenBoundary::isJustifiableGap(/*continues=*/false, /*noSpaceBefore=*/false,
                                             /*isSpaceToken=*/false),
             "Ordinary word gap was not justifiable"))
    return 1;

  if (!check(TokenBoundary::allowsBreak(/*continues=*/false, /*noSpaceBefore=*/true),
             "CJK zero-width gap was not breakable"))
    return 1;
  if (!check(TokenBoundary::isJustifiableGap(/*continues=*/false, /*noSpaceBefore=*/true,
                                             /*isSpaceToken=*/false),
             "CJK zero-width gap was not justifiable"))
    return 1;

  if (!check(!TokenBoundary::allowsBreak(/*continues=*/true, /*noSpaceBefore=*/false),
             "Ruby or attached boundary was breakable"))
    return 1;
  if (!check(!TokenBoundary::isJustifiableGap(/*continues=*/true, /*noSpaceBefore=*/false,
                                              /*isSpaceToken=*/false),
             "Ruby or attached boundary was justifiable"))
    return 1;

  if (!check(TokenBoundary::allowsBreak(/*continues=*/true, /*noSpaceBefore=*/true),
             "Explicit hyphen boundary was not breakable"))
    return 1;
  if (!check(!TokenBoundary::isJustifiableGap(/*continues=*/true, /*noSpaceBefore=*/true,
                                              /*isSpaceToken=*/false),
             "Explicit hyphen boundary was justifiable"))
    return 1;

  if (!check(TokenBoundary::isJustifiableGap(/*continues=*/true, /*noSpaceBefore=*/false,
                                             /*isSpaceToken=*/true),
             "Literal attached space was not justifiable"))
    return 1;

  constexpr uint32_t breakableCodepoints[] = {
      '-',    0x058A, 0x2010, 0x2012, 0x2013, 0x2014, 0x2015, 0x2043, 0x207B, 0x208B,
      0x2212, 0x2E17, 0x2E3A, 0x2E3B, 0xFE58, 0xFE63, 0xFF0D, 0x005F, 0x2026,
  };
  for (const uint32_t cp : breakableCodepoints) {
    if (!TokenBoundary::allowsBreakAfterExplicitHyphen(cp)) {
      std::cerr << "Visible explicit hyphen was not breakable: U+" << std::hex << cp << '\n';
      return 1;
    }
  }

  if (!check(!TokenBoundary::allowsBreakAfterExplicitHyphen(0x00AD), "Soft hyphen was treated as visible")) return 1;
  if (!check(!TokenBoundary::allowsBreakAfterExplicitHyphen(0x2011), "Non-breaking hyphen allowed a break"))
    return 1;
  if (!check(!TokenBoundary::allowsBreakAfterExplicitHyphen('.'), "Non-hyphen punctuation allowed a break")) return 1;

  return 0;
}
