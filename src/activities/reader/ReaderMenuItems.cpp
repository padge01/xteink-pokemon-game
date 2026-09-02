#include "ReaderMenuItems.h"

bool ReaderMenuItemList::append(const ReaderMenuAction action, const StrId labelId) {
  if (count >= items.size()) return false;
  items[count++] = {action, labelId};
  return true;
}

bool ReaderMenuItemList::contains(const ReaderMenuAction action) const { return indexOf(action) >= 0; }

int ReaderMenuItemList::indexOf(const ReaderMenuAction action) const {
  for (uint8_t index = 0; index < count; ++index) {
    if (items[index].action == action) return index;
  }
  return -1;
}

ReaderMenuTabs buildReaderMenuItems(const ReaderMenuBuildInput& input) {
  static_assert(READER_MENU_MAX_ITEMS == 12, "Review reader menu capacity before changing it");

  ReaderMenuTabs tabs;

  if (input.hasFootnotes) {
    tabs.main.append(ReaderMenuAction::FOOTNOTES, StrId::STR_FOOTNOTES);
  }
  if (input.hasDictionary) {
    tabs.main.append(ReaderMenuAction::LOOKUP, StrId::STR_LOOKUP);
    tabs.main.append(ReaderMenuAction::LOOKUP_HISTORY, StrId::STR_LOOKUP_HISTORY);
  }
  tabs.main.append(ReaderMenuAction::SELECT_CHAPTER, StrId::STR_SELECT_CHAPTER);
  tabs.main.append(ReaderMenuAction::READER_OPTIONS, StrId::STR_READER_OPTIONS);
  tabs.main.append(ReaderMenuAction::CONTROLS_OPTIONS, StrId::STR_CAT_CONTROLS);
  tabs.main.append(ReaderMenuAction::GO_TO_PERCENT, StrId::STR_GO_TO_PERCENT);
  tabs.main.append(ReaderMenuAction::AUTO_PAGE_TURN, StrId::STR_AUTO_TURN_INTERVAL_SECONDS);
  tabs.main.append(ReaderMenuAction::READING_STATS, StrId::STR_READING_STATS);
  tabs.main.append(ReaderMenuAction::TOGGLE_COMPLETED,
                   input.isBookCompleted ? StrId::STR_MARK_UNFINISHED : StrId::STR_MARK_FINISHED);

  tabs.bookmarks.append(ReaderMenuAction::SAVE_CLIPPING, StrId::STR_SAVE_CLIPPING);
  if (input.hasClippings) {
    tabs.bookmarks.append(ReaderMenuAction::VIEW_CLIPPINGS, StrId::STR_VIEW_CLIPPINGS);
  }
  tabs.bookmarks.append(ReaderMenuAction::BOOKMARK_TOGGLE,
                        input.isCurrentPageBookmarked ? StrId::STR_REMOVE_BOOKMARK : StrId::STR_ADD_BOOKMARK);
  if (input.hasBookmarks) {
    tabs.bookmarks.append(ReaderMenuAction::VIEW_BOOKMARKS, StrId::STR_VIEW_BOOKMARKS);
    tabs.bookmarks.append(ReaderMenuAction::DELETE_BOOKMARKS, StrId::STR_DELETE_BOOKMARKS);
  }
  tabs.bookmarks.append(ReaderMenuAction::SYNC, StrId::STR_SYNC_PROGRESS);
  tabs.bookmarks.append(ReaderMenuAction::NEARBY_POSITION_SYNC, StrId::STR_NEARBY_POSITION_SYNC);
  tabs.bookmarks.append(ReaderMenuAction::SEND_NEARBY_BOOK, StrId::STR_SEND_NEARBY_BOOK);
  tabs.bookmarks.append(ReaderMenuAction::SCREENSHOT, StrId::STR_SCREENSHOT_BUTTON);
  tabs.bookmarks.append(ReaderMenuAction::DISPLAY_QR, StrId::STR_DISPLAY_QR);

  tabs.settings.append(ReaderMenuAction::DELETE_STATS, StrId::STR_DELETE_BOOK_STATS);
  tabs.settings.append(ReaderMenuAction::DELETE_CACHE, StrId::STR_DELETE_CACHE);
  tabs.settings.append(ReaderMenuAction::SET_BOOK_DICTIONARY, StrId::STR_BOOK_DICTIONARY);
  if (input.showReadingPaceReset) {
    tabs.settings.append(ReaderMenuAction::RESET_READING_PACE, StrId::STR_RESET_READING_PACE);
  }

  return tabs;
}
