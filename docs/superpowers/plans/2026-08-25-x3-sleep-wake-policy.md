# X3 Sleep/Wake Policy Parity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port CrossInk upstream commit `f9e208ee` so UC8279 X3 devices only use seamless Quick Resume when a correctly sized saved frame exists, while preserving grayscale custom sleep images and transparent overlays.

**Architecture:** Put resume classification and the seamless-initialization decision in a small compile-time policy header so stale flags, invalid frames, and missing frames can be tested on the host. Keep SD access through `Storage`/`HalFile`, board detection through `BoardConfig`, and display initialization through the existing `setupDisplayAndFonts()` path. Adapt the upstream behavior to CrossInk's existing `BootResume::QuickResume` naming without changing user-facing settings or adding allocations.

**Tech Stack:** C++17/20, PlatformIO, existing host CMake tests, CrossInk HAL, ESP32-C3/X3 board profiles.

**Spec:** CrossInk upstream commit `f9e208ee` (`fix: prevent X3 sleep and wake screen artifacts`).

## Global Constraints

- Preserve ESP32-C3 stability and add no heap allocation.
- Use `Storage` and `HalFile`; do not access SdFat or SDK storage directly.
- Keep the change limited to UC8279 X3 saved-frame validation and BMP dithering parity.
- Preserve existing behavior for legacy X3, X4, simulator, silent restart, network restart, and cold boot paths.
- Run focused host tests and static checks only; defer full simulator/X3 builds to the batch checkpoint.
- Do not commit, flash, push, merge, or modify hardware.

---

### Task 1: Test and add the pure sleep/wake policy

**Files:**
- Create: `src/util/SleepWakePolicy.h`
- Create: `test/sleep_wake_policy/SleepWakePolicyTest.cpp`
- Create: `test/sleep_wake_policy/CMakeLists.txt`
- Modify: `test/CMakeLists.txt`

**Interfaces:**
- Produces: `SleepWakePolicy::Resume`, `resolveResume(bool, bool, bool, bool)`, `hasValidSavedFrame(bool, size_t, size_t)`, and `shouldInitializeSeamlessly(Resume, bool, bool)`.
- Consumes: no firmware state; all inputs are plain values so the policy stays host-testable.

- [ ] **Step 1: Write the failing policy test**

```cpp
#include <cstddef>

#include "src/util/SleepWakePolicy.h"

constexpr size_t expected = 52272;
static_assert(!SleepWakePolicy::shouldInitializeSeamlessly(
    SleepWakePolicy::Resume::QuickResume, true, false));
static_assert(SleepWakePolicy::resolveResume(false, false, false, false) ==
              SleepWakePolicy::Resume::Splash);
static_assert(SleepWakePolicy::resolveResume(false, false, true, false) ==
              SleepWakePolicy::Resume::QuickResume);
static_assert(SleepWakePolicy::hasValidSavedFrame(true, expected, expected));
static_assert(!SleepWakePolicy::hasValidSavedFrame(true, expected - 1, expected));
static_assert(SleepWakePolicy::shouldInitializeSeamlessly(
    SleepWakePolicy::Resume::Silent, true, false));
static_assert(SleepWakePolicy::shouldInitializeSeamlessly(
    SleepWakePolicy::Resume::QuickResume, false, false));

int main() { return 0; }
```

- [ ] **Step 2: Run the test and verify RED**

Run:

```bash
g++ -std=c++20 -Wall -Wextra -Werror -pedantic -I. test/sleep_wake_policy/SleepWakePolicyTest.cpp -o /tmp/SleepWakePolicyTest
```

Expected: compilation fails because `src/util/SleepWakePolicy.h` does not exist.

- [ ] **Step 3: Add the minimal policy implementation**

```cpp
#pragma once

#include <cstddef>
#include <cstdint>

namespace SleepWakePolicy {

enum class Resume : uint8_t { Splash, Silent, Network, QuickResume };

constexpr Resume resolveResume(bool networkResume, bool silentReboot, bool sleepWake, bool showBootScreen) {
  return networkResume    ? Resume::Network
         : silentReboot   ? Resume::Silent
         : sleepWake && !showBootScreen ? Resume::QuickResume
                                         : Resume::Splash;
}

constexpr bool hasValidSavedFrame(bool exists, size_t actualSize, size_t expectedSize) {
  return exists && actualSize == expectedSize;
}

constexpr bool shouldInitializeSeamlessly(Resume resume, bool isUc8279X3, bool hasValidFrame) {
  return resume != Resume::Splash &&
         !(resume == Resume::QuickResume && isUc8279X3 && !hasValidFrame);
}

}  // namespace SleepWakePolicy
```

- [ ] **Step 4: Run the test and verify GREEN**

Run the same compile command, then `/tmp/SleepWakePolicyTest`. Expected: both exit zero with no warnings.

### Task 2: Integrate saved-frame preflight into X3 boot

**Files:**
- Modify: `lib/hal/HalDisplay.h:21-31`
- Modify: `src/main.cpp:313-318`
- Modify: `src/main.cpp:608-629`
- Modify: `src/main.cpp:941-988`

**Interfaces:**
- Consumes: the policy from Task 1, `BoardConfig::ACTIVE.board`, `HalDisplay::X3_BUFFER_SIZE`, `Storage`, and `HalFile`.
- Produces: a wake-cause-gated `Resume`, a validated `hasValidSleepFrame` decision before display initialization, and a clean-refresh fallback when a preflighted frame cannot be loaded.

- [ ] **Step 1: Replace the local resume enum with the shared policy type**

Include `util/SleepWakePolicy.h` and use `using BootResume = SleepWakePolicy::Resume;`. Resolve it through `resolveResume()` so a stale persisted flag cannot create Quick Resume on a cold boot. Preserve the existing `QuickResume` enumerator so all current routing remains explicit and exhaustive.

- [ ] **Step 2: Expose the X3 frame size through the display HAL**

Add `HalDisplay::X3_BUFFER_SIZE` as a compile-time alias of the SDK constant. `main.cpp` must consume the HAL constant rather than reaching through to `EInkDisplay`.

- [ ] **Step 3: Add saved-frame preflight through the HAL**

Open `SLEEP_FRAME_FILE` through `Storage.openFileForRead`, read only `fileSize()`, and compare it with `HalDisplay::X3_BUFFER_SIZE` through `hasValidSavedFrame()`. Close the local `HalFile` before deleting an invalid frame. Log open and size failures before returning.

- [ ] **Step 4: Gate seamless UC8279 initialization**

Detect UC8279 with `gpio.deviceIsX3()` plus `BoardConfig::ACTIVE.board == BoardConfig::Board::XteinkX3Uc8279`. Preflight only the UC8279 Quick Resume path. Pass `SleepWakePolicy::shouldInitializeSeamlessly(...)` into `setupDisplayAndFonts()` and keep all non-UC8279 behavior unchanged.

- [ ] **Step 5: Add the clean recovery path**

If a frame passed preflight but fails its full read, remove it, clear the framebuffer, and perform one `HalDisplay::HALF_REFRESH` to establish a valid controller baseline. Do not allocate another framebuffer.

### Task 3: Correct BMP dithering roles and document the fix

**Files:**
- Modify: `src/activities/boot_sleep/SleepActivity.cpp:516-560`
- Modify: `src/activities/boot_sleep/SleepActivity.cpp:885-925`
- Modify: `CHANGELOG.md:5-12`

**Interfaces:**
- Custom sleep BMPs keep `Bitmap(file, true)` so grayscale detail is dithered into 1-bit output.
- Overlay BMPs use `Bitmap(file)` so white transparency is not polluted by error diffusion.

- [ ] **Step 1: Disable dithering only for overlay BMPs**

Keep both custom-sleep constructors dithered. Change only the overlay constructor and add a short comment explaining that error diffusion can make nominally white transparent pixels visible.

- [ ] **Step 2: Add the Unreleased changelog entry**

Document that grayscale custom sleep images retain detail and newer X3 displays recover cleanly when the saved wake frame is missing or invalid.

- [ ] **Step 3: Run focused verification**

Run the host policy test, `git diff --check`, and a source audit confirming storage remains HAL-routed and only the intended files changed. Do not run a full PlatformIO build in this slice.

- [ ] **Step 4: Report the hardware verification path**

On the physical X3, test Quick Resume with a valid frame, then with `/\.crosspoint/sleep_frame.bin` missing or truncated. Expected: valid frames resume without a splash; invalid frames produce one clean recovery refresh without retained stripes. Test custom grayscale and page-overlay BMP sleep screens separately.
