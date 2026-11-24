#include <Pokemon/PokemonTracker.h>
#include <gtest/gtest.h>

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

  verifier.request();
  EXPECT_FALSE(verifier.consume(turn));

  EXPECT_TRUE(verifier.renderSucceeded(123000, 64));
  ASSERT_TRUE(verifier.consume(turn));
  EXPECT_EQ(turn.renderedAtMs, 123000U);
  EXPECT_EQ(turn.bookProgressPercent, 64U);
  EXPECT_FALSE(verifier.consume(turn));
}

TEST(PokemonTurnVerifier, RerenderWithoutTurnIsNotCredited) {
  pokemon::PokemonTurnVerifier verifier;
  pokemon::VerifiedTurn turn{};

  // Menu, bookmark and overlay redraws render the page without a turn.
  EXPECT_FALSE(verifier.renderSucceeded(1000, 10));
  EXPECT_FALSE(verifier.consume(turn));

  verifier.request();
  EXPECT_TRUE(verifier.renderSucceeded(2000, 11));
  EXPECT_FALSE(verifier.renderSucceeded(3000, 11));
  ASSERT_TRUE(verifier.consume(turn));
  EXPECT_EQ(turn.renderedAtMs, 2000U);
  EXPECT_FALSE(verifier.consume(turn));
}

TEST(PokemonTurnVerifier, ProgressComesFromTheRenderedPage) {
  pokemon::PokemonTurnVerifier verifier;
  pokemon::VerifiedTurn turn{};

  // A chapter-crossing turn has no laid-out section when it is requested; the
  // percent must come from the render that follows, not from request time.
  verifier.request();
  EXPECT_TRUE(verifier.renderSucceeded(5000, 42));
  ASSERT_TRUE(verifier.consume(turn));
  EXPECT_EQ(turn.bookProgressPercent, 42U);

  verifier.request();
  EXPECT_TRUE(verifier.renderSucceeded(6000, 250));
  ASSERT_TRUE(verifier.consume(turn));
  EXPECT_EQ(turn.bookProgressPercent, 100U);
}

TEST(PokemonTurnVerifier, ResetDropsPendingAndUnconsumedTurns) {
  pokemon::PokemonTurnVerifier verifier;
  pokemon::VerifiedTurn turn{};

  verifier.request();
  EXPECT_TRUE(verifier.renderSucceeded(1000, 5));
  verifier.request();
  verifier.reset();
  EXPECT_FALSE(verifier.consume(turn));
  EXPECT_FALSE(verifier.renderSucceeded(2000, 6));
}

struct AttemptLog {
  uint32_t attempts = 0;
  bool succeed = false;
};

bool countAttempt(void* context, uint16_t, uint8_t) {
  auto& log = *static_cast<AttemptLog*>(context);
  ++log.attempts;
  return log.succeed;
}

// Keeps reading active and polls the checkpoint once per second, as the reader
// loop does, returning how many save attempts were made in [fromMs, toMs].
uint32_t pollEverySecond(pokemon::PokemonTracker& tracker, AttemptLog& log, const uint32_t fromMs,
                         const uint32_t toMs) {
  const uint32_t before = log.attempts;
  for (uint32_t nowMs = fromMs; nowMs <= toMs; nowMs += 1000) {
    if (nowMs % 60000U == 0) tracker.onSuccessfulPageTurn(nowMs);
    tracker.checkpointIfDue(nowMs);
  }
  return log.attempts - before;
}

TEST(PokemonTracker, FailedCheckpointBacksOffExponentially) {
  AttemptLog log;
  pokemon::PokemonTracker tracker(countAttempt, &log);
  tracker.beginSession();
  tracker.onSuccessfulPageTurn(0);

  // First attempt once five minutes are banked; then 30s, 60s, 120s, 300s
  // and a 600s ceiling instead of one SD write per loop tick.
  EXPECT_EQ(pollEverySecond(tracker, log, 0, 300000), 1U);
  EXPECT_EQ(tracker.consecutiveCommitFailures(), 1U);
  EXPECT_EQ(pollEverySecond(tracker, log, 301000, 329000), 0U);
  EXPECT_EQ(pollEverySecond(tracker, log, 330000, 330000), 1U);
  EXPECT_EQ(pollEverySecond(tracker, log, 331000, 389000), 0U);
  EXPECT_EQ(pollEverySecond(tracker, log, 390000, 390000), 1U);
  EXPECT_EQ(pollEverySecond(tracker, log, 391000, 509000), 0U);
  EXPECT_EQ(pollEverySecond(tracker, log, 510000, 510000), 1U);
  EXPECT_EQ(pollEverySecond(tracker, log, 511000, 809000), 0U);
  EXPECT_EQ(pollEverySecond(tracker, log, 810000, 810000), 1U);
  EXPECT_EQ(tracker.consecutiveCommitFailures(), 5U);
  EXPECT_EQ(pollEverySecond(tracker, log, 811000, 1409000), 0U);
  EXPECT_EQ(pollEverySecond(tracker, log, 1410000, 1410000), 1U);
  EXPECT_EQ(pollEverySecond(tracker, log, 1411000, 2009000), 0U);
  EXPECT_EQ(pollEverySecond(tracker, log, 2010000, 2010000), 1U);
  EXPECT_EQ(tracker.consecutiveCommitFailures(), 5U);
}

TEST(PokemonTracker, SuccessfulCheckpointResetsBackoff) {
  AttemptLog log;
  pokemon::PokemonTracker tracker(countAttempt, &log);
  tracker.beginSession();
  tracker.onSuccessfulPageTurn(0);

  EXPECT_EQ(pollEverySecond(tracker, log, 0, 300000), 1U);
  EXPECT_EQ(pollEverySecond(tracker, log, 301000, 330000), 1U);
  EXPECT_EQ(tracker.consecutiveCommitFailures(), 2U);

  log.succeed = true;
  EXPECT_EQ(pollEverySecond(tracker, log, 331000, 390000), 1U);
  EXPECT_EQ(tracker.consecutiveCommitFailures(), 0U);

  // Six minutes were saved at 390s, so five more are banked at 660s. That
  // failure starts again at the 30-second step.
  log.succeed = false;
  EXPECT_EQ(pollEverySecond(tracker, log, 391000, 660000), 1U);
  EXPECT_EQ(tracker.consecutiveCommitFailures(), 1U);
  EXPECT_EQ(pollEverySecond(tracker, log, 661000, 689000), 0U);
  EXPECT_EQ(pollEverySecond(tracker, log, 690000, 690000), 1U);
}

TEST(PokemonTracker, ExitFlushIgnoresBackoffAndNewSessionResetsIt) {
  AttemptLog log;
  pokemon::PokemonTracker tracker(countAttempt, &log);
  tracker.beginSession();
  tracker.onSuccessfulPageTurn(0);
  EXPECT_EQ(pollEverySecond(tracker, log, 0, 300000), 1U);

  // Leaving the reader always tries once, even inside a backoff window.
  EXPECT_FALSE(tracker.flushOnExit(301000));
  EXPECT_EQ(log.attempts, 2U);

  log.succeed = true;
  EXPECT_TRUE(tracker.beginSession());
  EXPECT_EQ(log.attempts, 3U);
  EXPECT_EQ(tracker.consecutiveCommitFailures(), 0U);
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
