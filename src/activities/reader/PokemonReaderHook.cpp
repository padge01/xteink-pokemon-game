#if defined(CROSSINK_ENABLE_POKEMON)

#include "PokemonReaderHook.h"

#include <Arduino.h>
#include <Logging.h>

#include <algorithm>
#include <optional>

#include "../RenderLock.h"
#include "pokemon/PokemonService.h"

namespace {

// Restart paths reach the hook from the render task or while already holding
// the lock; the main loop reaches it without one.
class RenderLockUnlessHeld {
 public:
  RenderLockUnlessHeld() {
    if (!RenderLock::heldByCurrentTask()) lock_.emplace();
  }

 private:
  std::optional<RenderLock> lock_;
};

uint8_t progressPercent(const int progressBasisPoints) {
  return static_cast<uint8_t>(std::clamp((progressBasisPoints + 50) / 100, 0, 100));
}

}  // namespace

void PokemonReaderHook::beginSession() {
  RenderLockUnlessHeld lock;
  verifier_.reset();
  finishedNotified_.store(false, std::memory_order_relaxed);
  atEndOfBook_ = false;
  pokemon::devicePokemonService().beginReadingSession();
}

void PokemonReaderHook::noteTurn(const bool forward, const bool isAuto) {
  (void)forward;
  if (isAuto) return;
  verifier_.request();
}

void PokemonReaderHook::rendered(const int progressBasisPoints) {
  verifier_.renderSucceeded(static_cast<uint32_t>(millis()), progressPercent(progressBasisPoints));
}

void PokemonReaderHook::renderedPage(const uint32_t pageIndex, const uint32_t pageCount) {
  rendered(pageCount == 0 ? 0 : static_cast<int>((pageIndex + 1ULL) * 10000ULL / pageCount));
}

void PokemonReaderHook::creditVerifiedTurn() {
  pokemon::VerifiedTurn turn{};
  if (!verifier_.consume(turn)) return;
  auto& service = pokemon::devicePokemonService();
  service.setBookProgressPercent(turn.bookProgressPercent);
  service.onSuccessfulPageTurn(turn.renderedAtMs);
}

void PokemonReaderHook::poll() {
  RenderLock lock(RenderLock::Mode::Try);
  if (!lock.ownsLock()) return;
  creditVerifiedTurn();
  pokemon::devicePokemonService().checkpointIfDue(static_cast<uint32_t>(millis()));
}

void PokemonReaderHook::flush() {
  RenderLockUnlessHeld lock;
  creditVerifiedTurn();
  pokemon::devicePokemonService().flushPending(static_cast<uint32_t>(millis()));
}

void PokemonReaderHook::exit() {
  RenderLockUnlessHeld lock;
  creditVerifiedTurn();
  pokemon::devicePokemonService().flushOnExit(static_cast<uint32_t>(millis()));
  verifier_.reset();
}

void PokemonReaderHook::onBookFinished(const std::string& bookPath, const std::string& title) {
  if (finishedNotified_.exchange(true, std::memory_order_relaxed)) return;
  LOG_INF("POKE", "Book finished: %s (%s)", title.c_str(), bookPath.c_str());
}

void PokemonReaderHook::noteEndOfBook(const bool atEndOfBook, const std::string& bookPath, const std::string& title) {
  if (atEndOfBook && !atEndOfBook_) onBookFinished(bookPath, title);
  atEndOfBook_ = atEndOfBook;
}

#endif
