# Directory Listing Error Parity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port CrossInk upstream commit `2a9fca60` so SD read failures cannot masquerade as complete directory listings in the device UI or file-transfer protocols.

**Architecture:** Extend `HalFile` with a sticky read-failure flag for the latest `openNextFile()` attempt, then translate its allocation/read flags into a small three-state `FsHelpers::DirectoryIterationResult`. All directory consumers must check the result after their loop. File-transfer endpoints preflight before streaming and verify newly created directories are actually enumerable before reporting success.

**Tech Stack:** C++20 host policy test, Arduino/ESP32-C3 firmware, SdFat through `HalStorage`, PlatformIO consumers.

**Spec:** CrossInk upstream commit `2a9fca60` (`fix: report incomplete directory listings (#610)`). The public issue body could not be retrieved anonymously; implementation intent is derived from the upstream commit diff and changelog.

## Global Constraints

- Preserve all existing dirty-worktree changes.
- Keep SD access behind `HalStorage`/`HalFile`; only `HalStorage.cpp` may inspect SdFat's `getError()`.
- Add no per-entry heap allocation beyond the existing fallible `HalFile` wrapper allocation.
- Add exactly two boolean state bytes to `HalFile`; clear/move them with the handle lifecycle.
- The two 256-byte filename buffers used during post-`mkdir` verification are bounded by FAT's 255-byte long-filename limit and run only in transfer activities, never the reader hot path.
- Use existing translated `STR_ERROR_GENERAL_FAILURE` for the on-device failure state; protocol/log strings remain hardcoded.
- Run focused host tests and static/source checks only. Defer simulator/X3 builds to the batch checkpoint.
- Do not commit, flash, push, merge, or modify hardware.

---

### Task 1: Test and add directory-iteration result policy

**Files:**
- Create: `lib/FsHelpers/DirectoryIterationResult.h`
- Create: `test/directory_iteration_result/DirectoryIterationResultTest.cpp`
- Create: `test/directory_iteration_result/CMakeLists.txt`
- Modify: `test/CMakeLists.txt`

**Interfaces:**
- Produces: `FsHelpers::DirectoryIterationResult { Complete, AllocationFailed, ReadFailed }`.
- Produces: `classifyDirectoryIterationEnd(bool allocationFailed, bool readFailed)` and `directoryIterationFailed(result)`.

- [ ] **Step 1: Write the failing compile-time test**

```cpp
#include "lib/FsHelpers/DirectoryIterationResult.h"

using FsHelpers::DirectoryIterationResult;
using FsHelpers::classifyDirectoryIterationEnd;

static_assert(classifyDirectoryIterationEnd(false, false) == DirectoryIterationResult::Complete);
static_assert(classifyDirectoryIterationEnd(true, false) == DirectoryIterationResult::AllocationFailed);
static_assert(classifyDirectoryIterationEnd(false, true) == DirectoryIterationResult::ReadFailed);
static_assert(classifyDirectoryIterationEnd(true, true) == DirectoryIterationResult::AllocationFailed);

int main() { return 0; }
```

- [ ] **Step 2: Verify RED**

Run `g++ -std=c++20 -Wall -Wextra -Werror -pedantic -I. test/directory_iteration_result/DirectoryIterationResultTest.cpp -o /tmp/DirectoryIterationResultTest` in WSL. Expected: missing-header compilation failure.

- [ ] **Step 3: Add the minimal constexpr result policy**

```cpp
enum class DirectoryIterationResult : uint8_t { Complete, AllocationFailed, ReadFailed };

constexpr DirectoryIterationResult classifyDirectoryIterationEnd(bool allocationFailed, bool readFailed) {
  if (allocationFailed) return DirectoryIterationResult::AllocationFailed;
  return readFailed ? DirectoryIterationResult::ReadFailed : DirectoryIterationResult::Complete;
}
```

- [ ] **Step 4: Verify GREEN**

Compile and run the same test with strict warnings. Expected: exit zero and no output.

### Task 2: Capture iteration errors in the storage HAL

**Files:**
- Modify: `lib/hal/HalStorage.h:73-112`
- Modify: `lib/hal/HalStorage.cpp:198-204`
- Modify: `lib/hal/HalStorage.cpp:375-403`

**Interfaces:**
- Produces: `HalFile::iterationFailed() const`.
- Consumes: SdFat `FsFile::getError()` only inside `HalStorage.cpp`.

- [ ] **Step 1: Add `iterationFailed_` lifecycle state**

Initialize it to false, transfer and clear it during move assignment, and clear it on `close()`. Reset both failure flags at the start of `openNextFile()`.

- [ ] **Step 2: Capture the SdFat read error**

When `openNextFile()` returns an invalid child, read the still-open parent directory's `getError()`. Set `iterationFailed_` and log the hexadecimal error only when it is nonzero. Clean EOF leaves both flags false.

- [ ] **Step 3: Preserve sticky failure semantics across rewind**

Keep SdFat's read error sticky: `rewindDirectory()` clears only wrapper-allocation failure. Callers must reopen a directory after an iteration failure.

### Task 3: Add HAL-routed filesystem verification helpers

**Files:**
- Modify: `lib/FsHelpers/FsHelpers.h:7-14`
- Modify: `lib/FsHelpers/FsHelpers.cpp:1-15`

**Interfaces:**
- Produces: `directoryIterationResult(const HalFile&)`, `directoryIterationFailed(const HalFile&)`, `directoryCanBeEnumerated(const char*)`, and `directoryEntryVisibility(const char*, const char*)`.
- Produces: `DirectoryEntryVisibility { Visible, Missing, IterationFailed }`.

- [ ] **Step 1: Translate HAL flags into the explicit result**

On hardware, classify `allocationFailed()` and `iterationFailed()`. On the simulator, classify allocation failure only because the host adapter cannot surface an SdFat error.

- [ ] **Step 2: Add complete-enumeration validation**

Open the directory through `Storage`, walk and explicitly close each `HalFile`, yield between entries, then return true only when the post-loop result is `Complete`.

- [ ] **Step 3: Add created-entry visibility validation**

Open the created path to obtain its stored FAT name, then enumerate the parent with two fixed 256-byte buffers and exact `strcmp`. Return `Visible`, `Missing`, or `IterationFailed`; log before returning read failure.

### Task 4: Propagate failures to index and on-device browser

**Files:**
- Modify: `lib/FileIndex/FileIndex.h:25-76`
- Modify: `lib/FileIndex/FileIndex.cpp:1-140`
- Modify: `lib/FileIndex/FileIndex.cpp:249-310`
- Modify: `src/activities/home/FileBrowserActivity.h:60-70`
- Modify: `src/activities/home/FileBrowserActivity.cpp:240-315`
- Modify: `src/activities/home/FileBrowserActivity.cpp:1100-1140`

**Interfaces:**
- Produces: `FileIndex::directoryReadFailed() const` and `FileBrowserActivity::fileListReadFailed`.

- [ ] **Step 1: Reject incomplete index scans/builds**

After both `openNextFile()` loops, inspect `directoryIterationResult(root)`. Treat allocation failure as the existing memory failure; mark `directoryReadFailed_` only for `ReadFailed`, and do not publish a partial index.

- [ ] **Step 2: Reject incomplete in-memory browser scans**

After the direct scan loop, clear partial entries and set `fileListReadFailed` when iteration did not reach clean EOF. Propagate the index flag when indexed scanning fails.

- [ ] **Step 3: Render the existing translated general-failure message**

For an empty list, keep memory error first, then directory-read failure, then the existing firmware/no-files empty states.

### Task 5: Propagate failures to file-transfer protocols

**Files:**
- Modify: `src/network/CrossPointWebServer.h:80-90`
- Modify: `src/network/CrossPointWebServer.cpp:580-760`
- Modify: `src/network/CrossPointWebServer.cpp:970-1045`
- Modify: `src/network/UsbSerialFileTransfer.cpp:275-390`
- Modify: `src/network/WebDAVHandler.cpp:200-285`
- Modify: `src/network/WebDAVHandler.cpp:460-520`

**Interfaces:**
- Changes: `CrossPointWebServer::scanFiles(...)` returns `bool`.
- File-list streams preflight enumeration before committing a success status, then abort the client if a second-pass read fails.
- `mkdir`/`MKCOL` success requires `DirectoryEntryVisibility::Visible`.

- [ ] **Step 1: Make web scans report completion**

Permit a null visitor for preflight, return false for open/not-directory/read failure, send HTTP 500 before streaming when preflight fails, and stop the client rather than closing a partial JSON array if the live pass fails.

- [ ] **Step 2: Verify web-created folders**

Require an existing parent, create the folder, verify it is enumerable, and roll back only a confirmed `Missing` result. Never roll back on `IterationFailed`, because visibility is unknown.

- [ ] **Step 3: Preflight and validate USB serial operations**

Return `ERR:list_failed` before `DIR:` when preflight fails; re-check the streamed pass before `END`. For recursive `mkdir`, record the pre-existing rollback boundary and remove only newly created directories after a confirmed `Missing` result.

- [ ] **Step 4: Preflight and validate WebDAV operations**

Return HTTP 500 before a depth listing when preflight fails; stop the client on a second-pass failure. Verify `MKCOL` visibility and use the same conservative rollback rule.

### Task 6: Changelog and focused verification

**Files:**
- Modify: `CHANGELOG.md:3-15`

- [ ] **Step 1: Add the Unreleased fix entry**

State that directory listings report SD read failures instead of hiding later files and that transfer-created folders are verified before success.

- [ ] **Step 2: Run focused validation**

Run the strict host policy test, `git diff --check`, call-site searches for every directory loop changed by this port, and a final intended-files status check. Do not run PlatformIO.

- [ ] **Step 3: Record hardware verification for the batch checkpoint**

On X3, browse a large SD directory and list it through web, USB serial, and WebDAV. Under an induced/reproduced SD read fault, expect the device UI to show general failure and protocols to return/abort with failure instead of presenting a valid partial list. Create folders through each transfer path and confirm they immediately appear in the parent listing.
