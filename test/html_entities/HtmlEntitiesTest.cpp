#include <cstring>

#include "htmlEntities.h"

int main() {
  const char* const apostrophe = lookupHtmlEntity("&apos;", 6);
  if (!apostrophe || std::strcmp(apostrophe, "'") != 0) return 1;

  // Keep the neighboring binary-search entries covered so a misplaced insertion
  // cannot make the new entity pass while breaking the sorted lookup table.
  const char* const ampersand = lookupHtmlEntity("&amp;", 5);
  const char* const aRing = lookupHtmlEntity("&aring;", 7);
  if (!ampersand || std::strcmp(ampersand, "&") != 0) return 2;
  if (!aRing || std::strcmp(aRing, "å") != 0) return 3;

  return 0;
}
