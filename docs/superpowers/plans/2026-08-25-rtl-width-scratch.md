# RTL Width Scratch Safety Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make reordered EPUB word-width scratch allocation fallible so low contiguous memory does not abort an X3 during bidirectional layout.

**Architecture:** Add a header-only, allocation-neutral copy helper that reports reserve and append failures while preserving literal visual order. Use it with a local `ArenaVector<uint16_t>` inside `ParsedText::extractLine()`, remove the persistent `std::vector<uint16_t>` member, and retain the upstream error-return behavior.

**Tech Stack:** C++20, `ArenaVector`, PlatformIO firmware code, standalone WSL `g++` host regression.

**Spec:** `docs/superpowers/plans/2026-08-25-rtl-width-scratch-spec.md`

## Global Constraints

- Each reordered line adds exactly `2 * visualOrderScratch.size()` bytes because each width is `uint16_t`; total width scratch retained by the operation-scoped arena is two bytes per reordered word processed by that layout operation.
- Runtime size can exceed the 256-byte stack guideline, so it must use the existing fallible scratch arena rather than stack storage.
- Each buffer is referenced only by its `extractLine()` call and all width scratch is reclaimed when the local `layoutArena` is destroyed at the end of the layout operation; it adds no `ParsedText` member capacity.
- Add no setting, translation, dependency, task, framebuffer, cache-format, or PSRAM assumption.
- Do not run PlatformIO, flash hardware, commit, push, or merge in this slice.
- The repo rule forbidding commits without explicit user authorization overrides the generic plan skill's commit-step recommendation.

---

### Task 1: Reordered-width failure regression

**Files:**
- Create: `lib/Epub/Epub/ReorderedWidthScratch.h`
- Create: `test/reordered_width_scratch/ReorderedWidthScratchTest.cpp`
- Create: `test/reordered_width_scratch/CMakeLists.txt`
- Modify: `test/CMakeLists.txt`

**Interfaces:**
- Consumes: A scratch container exposing `bool reserve(size_t)` and `bool push_back(uint16_t)`, plus indexable width and order containers.
- Produces: `ReorderedWidthScratch::BuildResult` and `build(Scratch&, const Widths&, const Order&)`.

- [ ] **Step 1: Write the failing regression**

Create a fixed-capacity test scratch type with independently controlled reserve and append limits. Use literal width array `{10, 20, 30}` and visual order `{2, 0, 1}`. Assert that success produces `{30, 10, 20}`, reserve rejection returns `ReserveFailed`, and rejection after one append returns `AppendFailed`.

```cpp
#include <array>
#include <cstddef>
#include <cstdint>

#include "lib/Epub/Epub/ReorderedWidthScratch.h"

using ReorderedWidthScratch::BuildResult;
using ReorderedWidthScratch::build;

class ControlledScratch {
 public:
  constexpr ControlledScratch(size_t reserveLimit, size_t appendLimit)
      : reserveLimit_(reserveLimit), appendLimit_(appendLimit) {}

  constexpr bool reserve(size_t requested) {
    if (requested > reserveLimit_ || requested > values_.size()) return false;
    capacity_ = requested;
    return true;
  }

  constexpr bool push_back(uint16_t value) {
    if (size_ >= capacity_ || size_ >= appendLimit_) return false;
    values_[size_++] = value;
    return true;
  }

  constexpr size_t size() const { return size_; }
  constexpr uint16_t operator[](size_t index) const { return values_[index]; }

 private:
  std::array<uint16_t, 3> values_{};
  size_t reserveLimit_ = 0;
  size_t appendLimit_ = 0;
  size_t capacity_ = 0;
  size_t size_ = 0;
};

constexpr std::array<uint16_t, 3> WIDTHS{10, 20, 30};
constexpr std::array<uint16_t, 3> VISUAL_ORDER{2, 0, 1};

constexpr bool preservesVisualOrder() {
  ControlledScratch scratch(3, 3);
  return build(scratch, WIDTHS, VISUAL_ORDER) == BuildResult::Success && scratch.size() == 3 &&
         scratch[0] == 30 && scratch[1] == 10 && scratch[2] == 20;
}

constexpr bool reportsReserveFailure() {
  ControlledScratch scratch(2, 3);
  return build(scratch, WIDTHS, VISUAL_ORDER) == BuildResult::ReserveFailed && scratch.size() == 0;
}

constexpr bool reportsAppendFailure() {
  ControlledScratch scratch(3, 1);
  return build(scratch, WIDTHS, VISUAL_ORDER) == BuildResult::AppendFailed && scratch.size() == 1;
}

static_assert(preservesVisualOrder());
static_assert(reportsReserveFailure());
static_assert(reportsAppendFailure());

int main() { return 0; }
```

- [ ] **Step 2: Run the regression and verify RED**

Run:

```powershell
wsl.exe -e g++ -std=c++20 -Wall -Wextra -Werror -pedantic -I /mnt/c/GitHub/crossink-x3-companion/.worktrees/fix-kosync-2xx-compatibility /mnt/c/GitHub/crossink-x3-companion/.worktrees/fix-kosync-2xx-compatibility/test/reordered_width_scratch/ReorderedWidthScratchTest.cpp -o /tmp/crossink_reordered_width_scratch_test
```

Expected: compilation fails because `lib/Epub/Epub/ReorderedWidthScratch.h` does not exist.

- [ ] **Step 3: Implement the minimal helper**

Create `BuildResult { Success, ReserveFailed, AppendFailed }`. In `build`, reserve `visualOrder.size()` once, then append `widths[sourceIndex]` for every literal order entry, returning the matching failure immediately.

- [ ] **Step 4: Register and verify GREEN**

Register `ReorderedWidthScratchTest` in the host CMake tree. Compile with the strict command from Step 2 and run:

```powershell
wsl.exe -e /tmp/crossink_reordered_width_scratch_test
```

Expected: compile and execution both exit `0`.

### Task 2: Integrate fallible scratch into ParsedText

**Files:**
- Modify: `lib/Epub/Epub/ParsedText.cpp:1573-1688`
- Modify: `lib/Epub/Epub/ParsedText.h:57-64`

**Interfaces:**
- Consumes: `ReorderedWidthScratch::build` and `BuildResult` from Task 1, plus the existing `scratchArena`.
- Produces: Recoverable `false` from `ParsedText::extractLine()` on reordered-width allocation failure.

- [ ] **Step 1: Allocate local fallible width scratch**

Include `ReorderedWidthScratch.h`. Inside the `willReorder` branch, construct `ArenaVector<uint16_t> reorderedWidths(scratchArena)` and call `build(reorderedWidths, lineWordWidths, visualOrderScratch)`.

- [ ] **Step 2: Log and return each failure**

For `ReserveFailed`, log `OOM allocating reordered word-width scratch (%u words)` with `visualOrderScratch.size()` and return `false`. For `AppendFailed`, log `OOM growing reordered word-width scratch` and return `false`.

- [ ] **Step 3: Replace width consumers and remove persistent storage**

Use `reorderedWidths` in both layout loops and width additions. Remove `reorderedWidthsScratch.clear()`, `.reserve()`, `.push_back()`, and `lineWordWidths.swap(reorderedWidthsScratch)`, then delete the member from `ParsedText.h`. Leave all other scratch members untouched.

- [ ] **Step 4: Re-run the focused regression**

Compile and run the strict host regression from Task 1. Expected: both exit `0`.

### Task 3: Changelog and focused verification

**Files:**
- Modify: `CHANGELOG.md`

**Interfaces:**
- Consumes: Completed fallible RTL scratch behavior.
- Produces: A user-facing Fixed entry.

- [ ] **Step 1: Add the changelog entry**

Add: `EPUB bidirectional text layout now exits safely instead of restarting when an X3/X4 cannot allocate reordered word-width scratch.`

- [ ] **Step 2: Run final focused checks**

Freshly compile and run the strict host regression, then run `git diff --check`. Expected: all exit `0`; do not run PlatformIO.

- [ ] **Step 3: Audit the heap mechanism**

Confirm the persistent `std::vector<uint16_t> reorderedWidthsScratch` member and all its uses are gone; confirm the local width payload is `sizeof(uint16_t) * visualOrderScratch.size()`, reserve and append failures log before returning, and no other allocation behavior changed.
