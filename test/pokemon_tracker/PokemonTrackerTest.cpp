#include <gtest/gtest.h>

#include <Pokemon/PokemonTracker.h>

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
