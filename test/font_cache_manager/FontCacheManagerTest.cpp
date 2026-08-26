#include <cstdlib>
#include <iostream>
#include <map>
#include <string>

#include <FontCacheManager.h>
#include <SdCardFont.h>

namespace {

void require(const bool condition, const char* message) {
  if (condition) return;
  std::cerr << message << '\n';
  std::exit(1);
}

void mixedFontsPrewarmIndependently() {
  const std::map<int, EpdFontFamily> builtInFonts;
  SdCardFont bodyFont;
  SdCardFont titleFont;
  const std::map<int, SdCardFont*> sdFonts{{101, &bodyFont}, {202, &titleFont}};
  FontCacheManager manager(builtInFonts, sdFonts);

  auto scope = manager.createPrewarmScope();
  manager.recordText("body", 101, EpdFontFamily::REGULAR);
  manager.recordText("title", 202, EpdFontFamily::BOLD);
  require(scope.endScanAndPrewarm(), "mixed-font prewarm reported failure");

  require(bodyFont.prewarmCalls.size() == 1, "body font was not prewarmed exactly once");
  require(bodyFont.prewarmCalls[0].text == "body", "body font received text belonging to another font");
  require(bodyFont.prewarmCalls[0].styleMask == 0x01, "body font received the wrong style mask");
  require(titleFont.prewarmCalls.size() == 1, "title font was not prewarmed exactly once");
  require(titleFont.prewarmCalls[0].text == "title", "title font received text belonging to another font");
  require(titleFont.prewarmCalls[0].styleMask == 0x02, "title font received the wrong style mask");
}

void negativeFontIdsKeepTheirOwnSlot() {
  constexpr int NEGATIVE_FONT_ID = -1853923692;
  constexpr int STATUS_FONT_ID = 674098198;
  const std::map<int, EpdFontFamily> builtInFonts;
  SdCardFont readerFont;
  SdCardFont statusFont;
  const std::map<int, SdCardFont*> sdFonts{{NEGATIVE_FONT_ID, &readerFont}, {STATUS_FONT_ID, &statusFont}};
  FontCacheManager manager(builtInFonts, sdFonts);

  auto scope = manager.createPrewarmScope();
  manager.recordText("Hangul body", NEGATIVE_FONT_ID, EpdFontFamily::REGULAR);
  manager.recordText("status", STATUS_FONT_ID, EpdFontFamily::REGULAR);
  require(scope.endScanAndPrewarm(), "negative-id prewarm reported failure");

  require(readerFont.prewarmCalls.size() == 1, "negative font id lost its scan slot");
  require(readerFont.prewarmCalls[0].text == "Hangul body", "negative font id was relabeled with status text");
  require(statusFont.prewarmCalls.size() == 1, "status font was not prewarmed");
  require(statusFont.prewarmCalls[0].text == "status", "status font inherited negative-id text");
}

void scanCapacityFallsBackAfterFourFonts() {
  const std::map<int, EpdFontFamily> builtInFonts;
  SdCardFont fonts[5];
  const std::map<int, SdCardFont*> sdFonts{
      {10, &fonts[0]}, {20, &fonts[1]}, {30, &fonts[2]}, {40, &fonts[3]}, {50, &fonts[4]}};
  FontCacheManager manager(builtInFonts, sdFonts);

  auto scope = manager.createPrewarmScope();
  manager.recordText("a", 10, EpdFontFamily::REGULAR);
  manager.recordText("b", 20, EpdFontFamily::REGULAR);
  manager.recordText("c", 30, EpdFontFamily::REGULAR);
  manager.recordText("d", 40, EpdFontFamily::REGULAR);
  manager.recordText("e", 50, EpdFontFamily::REGULAR);
  require(scope.endScanAndPrewarm(), "bounded font scan reported failure");

  for (size_t i = 0; i < 4; ++i) {
    require(fonts[i].prewarmCalls.size() == 1, "one of the four scan slots was skipped");
  }
  require(fonts[4].prewarmCalls.empty(), "fifth font unexpectedly displaced a bounded scan slot");
}

void smallCapsStayUppercaseWithinTheirFontEntry() {
  const std::map<int, EpdFontFamily> builtInFonts;
  SdCardFont font;
  const std::map<int, SdCardFont*> sdFonts{{303, &font}};
  FontCacheManager manager(builtInFonts, sdFonts);

  auto scope = manager.createPrewarmScope();
  manager.recordText("Mixed", 303,
                     static_cast<EpdFontFamily::Style>(EpdFontFamily::REGULAR | EpdFontFamily::SMALL_CAPS));
  require(scope.endScanAndPrewarm(), "small-caps prewarm reported failure");

  require(font.prewarmCalls.size() == 1, "small-caps font was not prewarmed");
  require(font.prewarmCalls[0].text == "MIXED", "small-caps scan did not record the rendered uppercase glyphs");
  require(font.prewarmCalls[0].styleMask == 0x01, "small-caps overlay changed the base style mask");
}

}  // namespace

int main() {
  mixedFontsPrewarmIndependently();
  negativeFontIdsKeepTheirOwnSlot();
  scanCapacityFallsBackAfterFourFonts();
  smallCapsStayUppercaseWithinTheirFontEntry();
  return 0;
}
