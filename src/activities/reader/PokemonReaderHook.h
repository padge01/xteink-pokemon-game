#pragma once

// The single seam between the three readers and the optional Pokemon companion.
//
// Readers declare a `PokemonReaderHook pokemonHook;` member only under
// CROSSINK_ENABLE_POKEMON and route every call through POKEMON_READER_HOOK, so
// stock CrossInk builds compile the call sites (and their arguments) away and
// carry no Pokemon state, flash, or work.
//
// Threading mirrors PluginReaderEvents: noteTurn() runs on the reader input
// path, rendered() on the render task, and everything else on the main loop
// or a render-locked restart path. Turn handoff uses the lock-free
// PokemonTurnVerifier; tracker/save work is serialized with RenderLock.
#if defined(CROSSINK_ENABLE_POKEMON)

#include <PokemonTracker.h>

#include <atomic>
#include <cstdint>
#include <string>

class PokemonReaderHook {
 public:
  // Reader entry. Opens a credit session and retries credit a failed exit left.
  void beginSession();
  // A manual turn changed the visible page. Automatic page turns are not reading
  // activity and are ignored; chapter/TOC/bookmark jumps never call this.
  void noteTurn(bool forward, bool isAuto);
  // Render task, after a book page (not a menu, overlay or end screen) reached
  // the panel. Confirms a pending turn with the rendered page's progress.
  void rendered(int progressBasisPoints);
  // rendered() for paged formats: progress through the end of pageIndex.
  void renderedPage(uint32_t pageIndex, uint32_t pageCount);
  // Main loop. Credits confirmed turns and saves due checkpoints; it skips the
  // tick rather than wait behind a render.
  void poll();
  // Saves credited whole minutes while keeping the session open. Used before
  // sleep and before silent restarts that bypass onExit().
  void flush();
  // Reader exit: saves credited whole minutes and closes the session.
  void exit();
  // Fires once per reader session when the book is finished: End-of-Book
  // screen reached, Mark as Finished, or the last TXT page rendered. Phase 2
  // (Book Memories) hangs off this seam; until then it only logs. It may run
  // on the render task, so implementations must not block on SD there.
  void onBookFinished(const std::string& bookPath, const std::string& title);
  // Per-frame end-of-book state from the reader loop; reports the false->true edge.
  void noteEndOfBook(bool atEndOfBook, const std::string& bookPath, const std::string& title);

 private:
  void creditVerifiedTurn();

  pokemon::PokemonTurnVerifier verifier_;
  std::atomic<bool> finishedNotified_{false};
  bool atEndOfBook_ = false;
};

#define POKEMON_READER_HOOK(hook, call) (hook).call
#else
#define POKEMON_READER_HOOK(hook, call) ((void)0)
#endif
