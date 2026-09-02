#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "I18nKeys.h"

enum class ReaderMenuAction : uint8_t {
  SELECT_CHAPTER,
  FOOTNOTES,
  GO_TO_PERCENT,
  AUTO_PAGE_TURN,
  ROTATE_SCREEN,
  SCREENSHOT,
  DISPLAY_QR,
  GO_HOME,
  SYNC,
  NEARBY_POSITION_SYNC,
  SEND_NEARBY_BOOK,
  DELETE_STATS,
  DELETE_CACHE,
  RESET_READING_PACE,
  READING_STATS,
  TOGGLE_COMPLETED,
  READER_OPTIONS,
  CONTROLS_OPTIONS,
  BOOKMARK_TOGGLE,
  VIEW_BOOKMARKS,
  DELETE_BOOKMARKS,
  SAVE_CLIPPING,
  VIEW_CLIPPINGS,
  LOOKUP,
  LOOKUP_HISTORY,
  SET_BOOK_DICTIONARY,
};

inline constexpr size_t READER_MENU_MAX_ITEMS = 12;

struct ReaderMenuItem {
  ReaderMenuAction action{};
  StrId labelId{};
};

struct ReaderMenuItemList {
  std::array<ReaderMenuItem, READER_MENU_MAX_ITEMS> items{};
  uint8_t count = 0;

  bool append(ReaderMenuAction action, StrId labelId);
  [[nodiscard]] bool contains(ReaderMenuAction action) const;
  [[nodiscard]] int indexOf(ReaderMenuAction action) const;
  [[nodiscard]] const ReaderMenuItem& operator[](size_t index) const { return items[index]; }
};

struct ReaderMenuTabs {
  ReaderMenuItemList main;
  ReaderMenuItemList bookmarks;
  ReaderMenuItemList settings;
};

struct ReaderMenuBuildInput {
  bool hasFootnotes = false;
  bool hasDictionary = false;
  bool hasBookmarks = false;
  bool hasClippings = false;
  bool isCurrentPageBookmarked = false;
  bool isBookCompleted = false;
  bool showReadingPaceReset = false;
};

[[nodiscard]] ReaderMenuTabs buildReaderMenuItems(const ReaderMenuBuildInput& input);
