#include <cstring>

#include "XmlParserUtils.h"

int main() {
  if (!xmlLocalNameEquals("package", "package")) return 1;
  if (!xmlLocalNameEquals("opf:package", "package")) return 2;
  if (!xmlLocalNameEquals("arbitrary:title", "title")) return 3;
  if (xmlLocalNameEquals("arbitrary:metadata", "package")) return 4;
  if (std::strcmp(xmlLocalName(nullptr), "") != 0) return 5;
  return 0;
}
