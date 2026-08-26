# Dictionary Lookup Parity Specification

## Goal

Adapt CrossPoint development commits `694bb95d` and `91663635` so CrossInk's shared dictionary flow behaves correctly on button-driven X3 readers and remains readable when the reader framebuffer contains Dark Mode content.

## Required behavior

- Declining the optional alternate-form lookup with Back or No completes the current reader-page lookup instead of restoring the selected word.
- The controller reports that decline as `NotFoundDismissedBack`, not `Cancelled`.
- `DictionaryWordSelectActivity` treats `NotFoundDismissedBack` as a terminal dismissal and returns to the reader. This adapts the upstream intent because this branch predates the upstream consumer change that made the event terminal.
- `Cancelled` retains its existing meaning: repaint and continue word selection after cancelling an in-progress lookup.
- Alternate-form prompts, dictionary-miss screens, and dictionary read-error screens clear the existing framebuffer before drawing. Looking-up toast overlays and idle rendering do not clear it.
- Existing behavior in `DictionaryDefinitionActivity` and `LookedUpWordsActivity` remains unchanged.

## Constraints

- Support ESP32-C3 X3/X4 builds without PSRAM.
- Add no heap allocation, persistent setting, translation string, dependency, task, or cache-format change.
- Reuse `GfxRenderer::clearScreen()` and the existing single framebuffer.
- Keep the policy host-testable without constructing SDK, display, input, storage, or FreeRTOS objects.
- Do not run PlatformIO, flash hardware, commit, push, or merge in this slice.

## Verification

- A strict C++20 host regression must prove the decline state/event transition, terminal word-selection disposition, retained cancellation behavior, and the clear/no-clear renderer policy.
- `git diff --check` must report no whitespace errors.
- Later batched X3 verification: enable Reader Dark Mode, look up a word that reaches the alternate-form prompt or not-found screen, verify all text is visible, then decline and confirm the reader page returns without an active word highlight.
