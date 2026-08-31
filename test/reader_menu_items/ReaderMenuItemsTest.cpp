#include <array>
#include <cstdint>
#include <cstdio>
#include <type_traits>

#include "activities/reader/ReaderMenuItems.h"

namespace {

int failures = 0;

void expect(const bool condition, const char* message) {
  if (condition) return;
  std::fprintf(stderr, "FAIL: %s\n", message);
  ++failures;
}

const ReaderMenuItem* findItem(const ReaderMenuItemList& list, const ReaderMenuAction action) {
  for (uint8_t index = 0; index < list.count; ++index) {
    if (list[index].action == action) return &list[index];
  }
  return nullptr;
}

void maximumConfigurationHasHandCheckedRows() {
  const ReaderMenuTabs tabs = buildReaderMenuItems({
      .hasFootnotes = true,
      .hasDictionary = true,
      .hasBookmarks = true,
      .hasClippings = true,
      .isCurrentPageBookmarked = true,
      .isBookCompleted = true,
      .showReadingPaceReset = true,
  });

  constexpr std::array mainActions{
      ReaderMenuAction::FOOTNOTES,         ReaderMenuAction::LOOKUP,
      ReaderMenuAction::LOOKUP_HISTORY,    ReaderMenuAction::SELECT_CHAPTER,
      ReaderMenuAction::READER_OPTIONS,    ReaderMenuAction::CONTROLS_OPTIONS,
      ReaderMenuAction::GO_TO_PERCENT,     ReaderMenuAction::AUTO_PAGE_TURN,
      ReaderMenuAction::READING_STATS,     ReaderMenuAction::TOGGLE_COMPLETED,
  };
  constexpr std::array bookmarkActions{
      ReaderMenuAction::SAVE_CLIPPING,       ReaderMenuAction::VIEW_CLIPPINGS,
      ReaderMenuAction::BOOKMARK_TOGGLE,     ReaderMenuAction::VIEW_BOOKMARKS,
      ReaderMenuAction::DELETE_BOOKMARKS,    ReaderMenuAction::SYNC,
      ReaderMenuAction::NEARBY_POSITION_SYNC, ReaderMenuAction::SEND_NEARBY_BOOK,
      ReaderMenuAction::SCREENSHOT,          ReaderMenuAction::DISPLAY_QR,
  };
  constexpr std::array settingsActions{
      ReaderMenuAction::DELETE_STATS,
      ReaderMenuAction::DELETE_CACHE,
      ReaderMenuAction::SET_BOOK_DICTIONARY,
      ReaderMenuAction::RESET_READING_PACE,
  };

  expect(tabs.main.count == mainActions.size(), "maximum main row count changed");
  expect(tabs.bookmarks.count == bookmarkActions.size(), "maximum bookmark row count changed");
  expect(tabs.settings.count == settingsActions.size(), "maximum settings row count changed");
  for (uint8_t index = 0; index < mainActions.size(); ++index) {
    expect(tabs.main[index].action == mainActions[index], "main row order changed");
  }
  for (uint8_t index = 0; index < bookmarkActions.size(); ++index) {
    expect(tabs.bookmarks[index].action == bookmarkActions[index], "bookmark row order changed");
  }
  for (uint8_t index = 0; index < settingsActions.size(); ++index) {
    expect(tabs.settings[index].action == settingsActions[index], "settings row order changed");
  }
}

void everyConditionalCombinationFitsAndSelectsCorrectLabels() {
  for (uint8_t bits = 0; bits < 128; ++bits) {
    const ReaderMenuBuildInput input{
        .hasFootnotes = (bits & (1U << 0U)) != 0,
        .hasDictionary = (bits & (1U << 1U)) != 0,
        .hasBookmarks = (bits & (1U << 2U)) != 0,
        .hasClippings = (bits & (1U << 3U)) != 0,
        .isCurrentPageBookmarked = (bits & (1U << 4U)) != 0,
        .isBookCompleted = (bits & (1U << 5U)) != 0,
        .showReadingPaceReset = (bits & (1U << 6U)) != 0,
    };
    const ReaderMenuTabs tabs = buildReaderMenuItems(input);

    expect(tabs.main.count == 7U + (input.hasFootnotes ? 1U : 0U) + (input.hasDictionary ? 2U : 0U),
           "main conditional count is wrong");
    expect(tabs.bookmarks.count == 7U + (input.hasBookmarks ? 2U : 0U) + (input.hasClippings ? 1U : 0U),
           "bookmark conditional count is wrong");
    expect(tabs.settings.count == 3U + (input.showReadingPaceReset ? 1U : 0U),
           "settings conditional count is wrong");
    expect(tabs.main.count <= READER_MENU_MAX_ITEMS, "main menu exceeded fixed capacity");
    expect(tabs.bookmarks.count <= READER_MENU_MAX_ITEMS, "bookmark menu exceeded fixed capacity");
    expect(tabs.settings.count <= READER_MENU_MAX_ITEMS, "settings menu exceeded fixed capacity");
    expect(tabs.main.contains(ReaderMenuAction::FOOTNOTES) == input.hasFootnotes,
           "footnotes conditional row is wrong");
    expect(tabs.main.contains(ReaderMenuAction::LOOKUP) == input.hasDictionary,
           "dictionary lookup conditional row is wrong");
    expect(tabs.main.contains(ReaderMenuAction::LOOKUP_HISTORY) == input.hasDictionary,
           "dictionary history conditional row is wrong");
    expect(tabs.bookmarks.contains(ReaderMenuAction::VIEW_CLIPPINGS) == input.hasClippings,
           "clippings conditional row is wrong");
    expect(tabs.bookmarks.contains(ReaderMenuAction::VIEW_BOOKMARKS) == input.hasBookmarks,
           "view bookmarks conditional row is wrong");
    expect(tabs.bookmarks.contains(ReaderMenuAction::DELETE_BOOKMARKS) == input.hasBookmarks,
           "delete bookmarks conditional row is wrong");
    expect(tabs.settings.contains(ReaderMenuAction::RESET_READING_PACE) == input.showReadingPaceReset,
           "reading pace conditional row is wrong");

    const ReaderMenuItem* bookmark = findItem(tabs.bookmarks, ReaderMenuAction::BOOKMARK_TOGGLE);
    const ReaderMenuItem* completion = findItem(tabs.main, ReaderMenuAction::TOGGLE_COMPLETED);
    expect(bookmark != nullptr, "bookmark toggle row is missing");
    expect(completion != nullptr, "completion toggle row is missing");
    if (bookmark) {
      expect(bookmark->labelId == (input.isCurrentPageBookmarked ? StrId::STR_REMOVE_BOOKMARK
                                                                : StrId::STR_ADD_BOOKMARK),
             "bookmark toggle label is wrong");
    }
    if (completion) {
      expect(completion->labelId == (input.isBookCompleted ? StrId::STR_MARK_UNFINISHED
                                                           : StrId::STR_MARK_FINISHED),
             "completion toggle label is wrong");
    }
  }
}

void appendRejectsOverflowWithoutChangingTheList() {
  ReaderMenuItemList list;
  for (size_t index = 0; index < READER_MENU_MAX_ITEMS; ++index) {
    expect(list.append(ReaderMenuAction::SELECT_CHAPTER, StrId::STR_SELECT_CHAPTER),
           "append rejected an in-capacity row");
  }
  expect(!list.append(ReaderMenuAction::FOOTNOTES, StrId::STR_FOOTNOTES),
         "append accepted a row beyond fixed capacity");
  expect(list.count == READER_MENU_MAX_ITEMS, "overflow changed fixed-list count");
  expect(list[READER_MENU_MAX_ITEMS - 1].action == ReaderMenuAction::SELECT_CHAPTER,
         "overflow changed the last valid row");
}

void indexOfReportsStableButtonNavigationTargets() {
  const ReaderMenuTabs basic = buildReaderMenuItems({});
  expect(basic.main.indexOf(ReaderMenuAction::SELECT_CHAPTER) == 0,
         "chapter row index is wrong in the basic menu");
  expect(basic.main.indexOf(ReaderMenuAction::READER_OPTIONS) == 1,
         "reader options row index is wrong in the basic menu");
  expect(basic.main.indexOf(ReaderMenuAction::FOOTNOTES) == -1,
         "missing optional row returned a valid index");

  const ReaderMenuTabs expanded = buildReaderMenuItems({
      .hasFootnotes = true,
      .hasDictionary = true,
  });
  expect(expanded.main.indexOf(ReaderMenuAction::READER_OPTIONS) == 4,
         "reader options row index did not follow optional rows");
}

static_assert(sizeof(ReaderMenuItemList) <= sizeof(ReaderMenuItem) * READER_MENU_MAX_ITEMS + 4U);
static_assert(std::is_trivially_destructible_v<ReaderMenuItemList>);

}  // namespace

int main() {
  maximumConfigurationHasHandCheckedRows();
  everyConditionalCombinationFitsAndSelectsCorrectLabels();
  appendRejectsOverflowWithoutChangingTheList();
  indexOfReportsStableButtonNavigationTargets();
  return failures == 0 ? 0 : 1;
}
