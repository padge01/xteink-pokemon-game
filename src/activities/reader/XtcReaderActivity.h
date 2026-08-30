/**
 * XtcReaderActivity.h
 *
 * XTC ebook reader activity for CrossPoint Reader
 * Displays pre-rendered XTC pages on e-ink display
 */

#pragma once

#include <Xtc.h>

#include <string>
#include <utility>

#include "BookReadingStats.h"
#include "EndOfBookOptions.h"
#include "GlobalReadingStats.h"
#include "ReaderProgressSaveDebouncer.h"
#include "activities/Activity.h"
#if defined(CROSSINK_ENABLE_POKEMON)
#include <PokemonTracker.h>
#endif

class XtcReaderActivity final : public Activity {
  std::shared_ptr<Xtc> xtc;

  uint32_t currentPage = 0;
  int pagesUntilFullRefresh = 0;
  uint32_t lastPageTurnTime = 0;
  unsigned long pageShownAtMs = 0UL;
  uint32_t sessionReadingSeconds = 0;
  BookReadingStats stats;
  GlobalReadingStats globalStats;
  ReadingStatsDateTime sessionStartLocalDateTime;
  bool hasSessionStartLocalDateTime = false;
  bool longPowerPageTurnHandled = false;
  bool frontButtonLongPressHandled = false;
  bool longPressBackHandled = false;
  ReaderProgressSaveDebouncer progressSaveDebouncer;
#if defined(CROSSINK_ENABLE_POKEMON)
  pokemon::PokemonTurnVerifier pokemonTurnVerifier;
#endif
  // Next-book suggestion menu for the End-of-Book screen
  EndOfBookOptions endOfBookOptions;

  enum class StatusBarOverlayPosition { Bottom, Top };
  struct StatusBarInfo {
    int currentPage;
    int pageCount;
    std::string title;
  };

  bool renderPage(uint32_t pageToRender);
  void renderStatusBarOverlay(StatusBarOverlayPosition position, uint32_t pageToRender) const;
  StatusBarInfo getStatusBarInfo(uint32_t pageToRender) const;
  bool saveProgress(uint32_t page);
  bool queueProgressSave(uint32_t pageToRender);
  bool flushQueuedProgress();
  void loadProgress();
  void pauseReadingStatsTimer(const char* source = "unknown");
  void resumeReadingStatsTimer(const char* source = "unknown");
  bool currentPageReadingSecondsForStats(uint32_t& seconds, const char* source) const;
  bool forwardPageReadElapsed(uint32_t& seconds, const char* source) const;
  void recordCurrentPageReadingTime(const char* source = "unknown");
  void recordForwardPageTurn(uint32_t seconds, bool recordPace);
  bool formatTimeLeftLabel(char* buf, size_t len, uint32_t pageToRender) const;
  void commitReadingStats();
  void resetCurrentBookStatsAfterDelete();
  void setBookCompleted(bool isCompleted);
  float getCurrentBookProgressPercent() const;
  void openChapterSelection();
  void openReadingStats();
  void deleteBookStats();
  void deleteBookCache();
  void openReaderMenu();
  void onReaderMenuConfirm(int action);
  bool executeLongPressBackAction();

 public:
  explicit XtcReaderActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::unique_ptr<Xtc> xtc,
                             int initialRefreshCountdown)
      : Activity("XtcReader", renderer, mappedInput),
        xtc(std::move(xtc)),
        pagesUntilFullRefresh(initialRefreshCountdown) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool isReaderActivity() const override { return true; }
  bool canSnapshotForSleepOverlay() const override { return true; }
  bool handlesReaderPowerSettingsOverride() const override { return true; }
  bool openReaderSettingsMenu() override {
    if (!xtc) {
      return false;
    }
    openReaderMenu();
    return true;
  }
  std::string getCurrentBookPath() const override { return xtc ? xtc->getPath() : std::string{}; }

  // Renders the last saved page to the frame buffer without flushing to display.
  // Used by SleepActivity to prepare the background for the overlay sleep mode.
  // Returns false if the page cannot be loaded (missing cache / file error).
  static bool drawCurrentPageToBuffer(const std::string& filePath, GfxRenderer& renderer);
  ScreenshotInfo getScreenshotInfo() const override;
};
