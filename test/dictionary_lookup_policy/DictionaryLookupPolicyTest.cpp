#include "src/util/DictionaryLookupPolicy.h"

using namespace DictionaryLookupPolicy;

struct RecordingRenderer {
  int clearCount = 0;

  constexpr void clearScreen() { ++clearCount; }
};

constexpr bool declineCompletesReaderLookup() {
  LookupState state = LookupState::AltFormPrompt;
  return declineAltForm(state) == LookupEvent::NotFoundDismissedBack && state == LookupState::Idle;
}

constexpr bool fullScreenPoliciesAreBounded() {
  RecordingRenderer renderer;
  const bool idle = prepareFullScreenOverlay(renderer, LookupState::Idle);
  const bool lookingUp = prepareFullScreenOverlay(renderer, LookupState::LookingUp);
  const bool altForm = prepareFullScreenOverlay(renderer, LookupState::AltFormPrompt);
  const bool notFound = prepareFullScreenOverlay(renderer, LookupState::NotFound);
  const bool readError = prepareFullScreenOverlay(renderer, LookupState::ReadError);
  return !idle && !lookingUp && altForm && notFound && readError && renderer.clearCount == 3;
}

static_assert(declineCompletesReaderLookup());
static_assert(exitsWordSelection(LookupEvent::NotFoundDismissedBack));
static_assert(exitsWordSelection(LookupEvent::NotFoundDismissedDone));
static_assert(!exitsWordSelection(LookupEvent::Cancelled));
static_assert(fullScreenPoliciesAreBounded());

int main() { return 0; }
