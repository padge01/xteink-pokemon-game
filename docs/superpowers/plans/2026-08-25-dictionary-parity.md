# Dictionary Lookup Parity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make alternate-form declines exit reader-page word selection and make full-screen dictionary feedback readable over Dark Mode content.

**Architecture:** Introduce a lightweight `constexpr` policy header that owns the lookup state/event enums and the three decisions this parity slice changes: the decline transition, terminal word-selection dismissals, and full-screen overlay preparation. Keep `DictionaryLookupController`'s public nested type names as aliases so existing consumers remain source-compatible, then route the real controller and word-selection activity through the tested policy.

**Tech Stack:** C++20, PlatformIO firmware code, standalone WSL `g++` host regression.

**Spec:** `docs/superpowers/plans/2026-08-25-dictionary-parity-spec.md`

## Global Constraints

- Support ESP32-C3 X3/X4 builds without PSRAM.
- Add no heap allocation, persistent setting, translation string, dependency, task, or cache-format change.
- Reuse `GfxRenderer::clearScreen()` and the existing single framebuffer.
- Keep the policy host-testable without constructing SDK, display, input, storage, or FreeRTOS objects.
- Do not run PlatformIO, flash hardware, commit, push, or merge in this slice.
- The repo rule forbidding commits without explicit user authorization overrides the generic plan skill's commit-step recommendation.

---

### Task 1: Dictionary lookup policy regression

**Files:**
- Create: `src/util/DictionaryLookupPolicy.h`
- Create: `test/dictionary_lookup_policy/DictionaryLookupPolicyTest.cpp`
- Create: `test/dictionary_lookup_policy/CMakeLists.txt`
- Modify: `test/CMakeLists.txt`

**Interfaces:**
- Consumes: A renderer type exposing `void clearScreen()`.
- Produces: `DictionaryLookupPolicy::LookupState`, `LookupEvent`, `declineAltForm(LookupState&)`, `exitsWordSelection(LookupEvent)`, and `prepareFullScreenOverlay(Renderer&, LookupState)`.

- [ ] **Step 1: Write the failing regression**

Create a strict compile-time test using literal expected values. It must assert that declining from `AltFormPrompt` moves to `Idle` and returns `NotFoundDismissedBack`; `NotFoundDismissedBack` and `NotFoundDismissedDone` exit word selection while `Cancelled` does not; and a fake renderer is cleared exactly once for `AltFormPrompt`, `NotFound`, and `ReadError` but never for `Idle` or `LookingUp`.

```cpp
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
```

- [ ] **Step 2: Run the regression and verify RED**

Run:

```powershell
wsl.exe -e g++ -std=c++20 -Wall -Wextra -Werror -pedantic -I /mnt/c/GitHub/crossink-x3-companion/.worktrees/fix-kosync-2xx-compatibility /mnt/c/GitHub/crossink-x3-companion/.worktrees/fix-kosync-2xx-compatibility/test/dictionary_lookup_policy/DictionaryLookupPolicyTest.cpp -o /tmp/crossink_dictionary_lookup_policy_test
```

Expected: compilation fails because `src/util/DictionaryLookupPolicy.h` does not exist.

- [ ] **Step 3: Implement the minimal allocation-free policy**

Create the two enum classes with the controller's existing members. Implement the decline transition as an `Idle` assignment plus the terminal-back event. Implement `exitsWordSelection` for only the two not-found dismissal events. Implement `prepareFullScreenOverlay` as a template that returns `false` without touching the renderer for `Idle` and `LookingUp`; for the other three states, call `renderer.clearScreen()` once and return `true`.

- [ ] **Step 4: Register and verify GREEN**

Add the standalone executable to `test/CMakeLists.txt`, compile with the strict command from Step 2, then run:

```powershell
wsl.exe -e /tmp/crossink_dictionary_lookup_policy_test
```

Expected: compile and execution both exit `0` with no output.

### Task 2: Integrate the tested policy

**Files:**
- Modify: `src/util/DictionaryLookupController.h`
- Modify: `src/util/DictionaryLookupController.cpp`
- Modify: `src/activities/reader/DictionaryWordSelectActivity.cpp`

**Interfaces:**
- Consumes: All interfaces produced by Task 1.
- Produces: The existing `DictionaryLookupController::LookupState` and `LookupEvent` names as aliases, preserving all current consumers.

- [ ] **Step 1: Preserve controller API names through policy aliases**

Include `DictionaryLookupPolicy.h` in the controller header and replace the nested enum definitions with public aliases:

```cpp
using LookupState = DictionaryLookupPolicy::LookupState;
using LookupEvent = DictionaryLookupPolicy::LookupEvent;
```

- [ ] **Step 2: Route decline and rendering through the policy**

In the alternate-form decline branch, retain `nextIsSuggestion = false` and return `DictionaryLookupPolicy::declineAltForm(state)`. In `render()`, after the `LookingUp` early return and before drawing the full-screen branches, call `DictionaryLookupPolicy::prepareFullScreenOverlay(renderer, state)`.

- [ ] **Step 3: Adapt the word-selection consumer**

Store `controller.handleInput()` in a local `lookupEvent`. Before its existing switch, use `DictionaryLookupPolicy::exitsWordSelection(lookupEvent)` to set an empty result, finish, and return. Remove the now-redundant `NotFoundDismissedBack` and `NotFoundDismissedDone` switch cases. Leave `Cancelled` as repaint-and-continue.

- [ ] **Step 4: Re-run the focused regression**

Compile and run the strict host test from Task 1. Expected: both exit `0` with no warnings or output.

### Task 3: User-facing record and focused verification

**Files:**
- Modify: `CHANGELOG.md`

**Interfaces:**
- Consumes: Completed controller and activity behavior.
- Produces: One user-facing Fixed entry covering both dictionary parity fixes.

- [ ] **Step 1: Update the changelog**

Add a Fixed entry stating that declining alternate dictionary-form lookup returns to the reader and dictionary prompt/miss screens remain readable in Dark Mode.

- [ ] **Step 2: Run final focused checks**

Run the strict host compile and executable once more, then run:

```powershell
git diff --check
```

Expected: the host test exits `0`, and Git reports no whitespace errors. Do not run PlatformIO in this slice.

- [ ] **Step 3: Audit resource and scope constraints**

Inspect the new policy header and changed call sites. Confirm there is no `new`, `malloc`, container, string, setting, translation, task, or cache change; confirm the worktree contains only this slice plus the previously approved dirty parity slices.
