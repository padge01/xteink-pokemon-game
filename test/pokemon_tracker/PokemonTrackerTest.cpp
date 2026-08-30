#include <gtest/gtest.h>

#include <Pokemon/PokemonTracker.h>

#include <algorithm>
#include <array>
#include <cstdint>

namespace {

struct CreditLog {
  struct Entry {
    uint16_t minutes;
    uint8_t progress;
  };

  std::array<Entry, 4> entries{};
  size_t count = 0;
  bool succeed = true;
};

bool recordCredit(void* context, const uint16_t minutes, const uint8_t progress) {
  auto& log = *static_cast<CreditLog*>(context);
  if (!log.succeed || log.count >= log.entries.size()) return false;
  log.entries[log.count++] = {minutes, progress};
  return true;
}

// Test-only behavioral reference for Joshua Miller's SessionAccumulator in
// crosspoint-reader-companion/lib/Companion/CompanionMood.cpp. Keeping this
// independent from PokemonTracker makes timeline drift visible.
class JoshuaSessionAccumulatorReference {
 public:
  void onPageTurn(const uint32_t nowSeconds) {
    if (!started_) {
      started_ = true;
      lastTickSeconds_ = nowSeconds;
    }
    lastPageTurnSeconds_ = nowSeconds;
    sawPageTurn_ = true;
  }

  void onTick(const uint32_t nowSeconds) {
    if (!started_) {
      started_ = true;
      lastTickSeconds_ = nowSeconds;
      return;
    }
    if (nowSeconds < lastTickSeconds_) {
      lastTickSeconds_ = nowSeconds;
      return;
    }

    uint32_t delta = nowSeconds - lastTickSeconds_;
    lastTickSeconds_ = nowSeconds;
    if (!sawPageTurn_ || nowSeconds - lastPageTurnSeconds_ > ACTIVE_WINDOW_SECONDS) return;
    delta = std::min<uint32_t>(delta, ACTIVE_WINDOW_SECONDS);
    creditedSeconds_ += delta;
  }

  uint32_t creditedSeconds() const { return creditedSeconds_; }

 private:
  static constexpr uint32_t ACTIVE_WINDOW_SECONDS = 300;
  uint32_t lastTickSeconds_ = 0;
  uint32_t lastPageTurnSeconds_ = 0;
  uint32_t creditedSeconds_ = 0;
  bool started_ = false;
  bool sawPageTurn_ = false;
};

TEST(PokemonTracker, MatchesJoshuaReferenceAcrossReaderTimelines) {
  constexpr std::array<const char*, 3> readerFormats = {"EPUB", "TXT", "XTC"};
  constexpr std::array<uint32_t, 8> renderedTurnsSeconds = {0, 35, 95, 250, 610, 625, 920, 925};
  constexpr std::array<uint32_t, 12> ticksSeconds = {0, 30, 60, 120, 250, 300, 600, 610, 625, 900, 920, 1226};

  for (const char* readerFormat : readerFormats) {
    SCOPED_TRACE(readerFormat);
    CreditLog log;
    pokemon::PokemonTracker tracker(recordCredit, &log);
    JoshuaSessionAccumulatorReference reference;
    ASSERT_TRUE(tracker.beginSession());

    size_t turnIndex = 0;
    for (const uint32_t tickSeconds : ticksSeconds) {
      while (turnIndex < renderedTurnsSeconds.size() && renderedTurnsSeconds[turnIndex] <= tickSeconds) {
        const uint32_t turnSeconds = renderedTurnsSeconds[turnIndex++];
        tracker.onSuccessfulPageTurn(turnSeconds * 1000U);
        reference.onPageTurn(turnSeconds);
      }
      tracker.checkpointIfDue(tickSeconds * 1000U);
      reference.onTick(tickSeconds);
      EXPECT_EQ(tracker.creditedSeconds(), reference.creditedSeconds());
    }
  }
}

TEST(PokemonTracker, MatchesJoshuaTrailingWindowForSustainedReading) {
  CreditLog log;
  pokemon::PokemonTracker tracker(recordCredit, &log);
  tracker.beginSession();
  tracker.setBookProgressPercent(64);

  for (uint32_t seconds = 0; seconds <= 600; seconds += 30) {
    tracker.onSuccessfulPageTurn(seconds * 1000U);
    tracker.checkpointIfDue(seconds * 1000U);
  }

  ASSERT_EQ(log.count, 2U);
  EXPECT_EQ(log.entries[0].minutes, 5U);
  EXPECT_EQ(log.entries[1].minutes, 5U);
  EXPECT_EQ(log.entries[0].progress, 64U);
  EXPECT_EQ(tracker.creditedSeconds(), 600U);
}

TEST(PokemonTracker, IdleOpenBookNeverPassesOneActiveWindow) {
  CreditLog log;
  pokemon::PokemonTracker tracker(recordCredit, &log);
  tracker.beginSession();
  tracker.onSuccessfulPageTurn(0);

  for (uint32_t seconds = 30; seconds <= 7200; seconds += 30) {
    tracker.checkpointIfDue(seconds * 1000U);
  }

  ASSERT_EQ(log.count, 1U);
  EXPECT_EQ(log.entries[0].minutes, 5U);
  EXPECT_EQ(tracker.creditedSeconds(), 300U);
}

TEST(PokemonTracker, ExitFlushesOnlyCompleteUncommittedMinutes) {
  CreditLog log;
  pokemon::PokemonTracker tracker(recordCredit, &log);
  tracker.beginSession();
  tracker.setBookProgressPercent(27);
  tracker.onSuccessfulPageTurn(0);

  tracker.flushOnExit(179000);

  ASSERT_EQ(log.count, 1U);
  EXPECT_EQ(log.entries[0].minutes, 2U);
  EXPECT_EQ(log.entries[0].progress, 27U);
}

TEST(PokemonTracker, FailedCommitRemainsPendingForExitRetry) {
  CreditLog log;
  log.succeed = false;
  pokemon::PokemonTracker tracker(recordCredit, &log);
  tracker.beginSession();
  tracker.onSuccessfulPageTurn(0);
  tracker.checkpointIfDue(300000);
  EXPECT_EQ(log.count, 0U);

  log.succeed = true;
  tracker.flushOnExit(301000);

  ASSERT_EQ(log.count, 1U);
  EXPECT_EQ(log.entries[0].minutes, 5U);
}

TEST(PokemonTracker, FailedExitCommitSurvivesTheNextSession) {
  CreditLog log;
  log.succeed = false;
  pokemon::PokemonTracker tracker(recordCredit, &log);
  tracker.beginSession();
  tracker.setBookProgressPercent(73);
  tracker.onSuccessfulPageTurn(0);
  EXPECT_FALSE(tracker.flushOnExit(179000));

  EXPECT_FALSE(tracker.beginSession());
  log.succeed = true;
  EXPECT_TRUE(tracker.beginSession());

  ASSERT_EQ(log.count, 1U);
  EXPECT_EQ(log.entries[0].minutes, 2U);
  EXPECT_EQ(log.entries[0].progress, 73U);
}

TEST(PokemonTurnVerifier, ReportsOnlyRenderedRequestedTurns) {
  pokemon::PokemonTurnVerifier verifier;
  pokemon::VerifiedTurn turn{};

  verifier.request(64);
  EXPECT_FALSE(verifier.consume(turn));

  verifier.renderSucceeded(123000);
  ASSERT_TRUE(verifier.consume(turn));
  EXPECT_EQ(turn.renderedAtMs, 123000U);
  EXPECT_EQ(turn.bookProgressPercent, 64U);
  EXPECT_FALSE(verifier.consume(turn));
}

TEST(PokemonTracker, BackwardsOrWrappedClockBanksNoBogusTime) {
  CreditLog log;
  pokemon::PokemonTracker tracker(recordCredit, &log);
  tracker.beginSession();
  tracker.onSuccessfulPageTurn(1000000);
  tracker.checkpointIfDue(1060000);
  EXPECT_EQ(tracker.creditedSeconds(), 60U);

  tracker.checkpointIfDue(500000);
  EXPECT_EQ(tracker.creditedSeconds(), 60U);
  tracker.onSuccessfulPageTurn(500000);
  tracker.checkpointIfDue(530000);
  EXPECT_EQ(tracker.creditedSeconds(), 90U);
}

}  // namespace
