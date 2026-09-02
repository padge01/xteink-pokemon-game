#pragma once

namespace DictionaryLookupPolicy {

enum class LookupState { Idle, LookingUp, AltFormPrompt, NotFound, ReadError };

enum class LookupEvent {
  None,
  FoundDefinition,
  NotFoundDismissedBack,
  NotFoundDismissedDone,
  SwitchDictionary,
  Cancelled
};

constexpr LookupEvent declineAltForm(LookupState& state) {
  state = LookupState::Idle;
  return LookupEvent::NotFoundDismissedBack;
}

constexpr bool exitsWordSelection(const LookupEvent event) {
  return event == LookupEvent::NotFoundDismissedBack || event == LookupEvent::NotFoundDismissedDone;
}

template <typename Renderer>
constexpr bool prepareFullScreenOverlay(Renderer& renderer, const LookupState state) {
  switch (state) {
    case LookupState::AltFormPrompt:
    case LookupState::NotFound:
    case LookupState::ReadError:
      renderer.clearScreen();
      return true;
    case LookupState::Idle:
    case LookupState::LookingUp:
      return false;
  }
  return false;
}

}  // namespace DictionaryLookupPolicy
