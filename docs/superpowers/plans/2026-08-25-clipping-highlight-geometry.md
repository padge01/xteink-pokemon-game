# Clipping Highlight Geometry Parity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port CrossInk upstream commit `3f06ace4` so saved EPUB clipping markers remain visually continuous when selected visible words are separated by hidden layout-space tokens.

**Architecture:** Put the gap decision in a small header-only geometry helper that accepts plain integer rectangles and can be tested without the renderer. During clipping highlight drawing, remember only the previous selected visible word and fill a positive same-line gap when its page-word index is immediately before the current one. Preserve all existing text matching, word rendering, dark-mode contrast, and line-boundary behavior.

**Tech Stack:** C++17/20, PlatformIO, existing host-test layout, CrossInk EPUB renderer.

**Spec:** CrossInk upstream commit `3f06ace4` (`fix: keep clipping highlights continuous through ellipses`).

## Global Constraints

- Preserve ESP32-C3 stability and add no heap allocation, persistent RAM, setting, dependency, or cache-format change.
- Keep the fix inside clipping highlight geometry and the existing EPUB rendering path.
- Fill only positive horizontal gaps between consecutive selected visible words on the same rendered line.
- Support both LTR and RTL word order, but do not bridge different lines, overlapping geometry, separate text blocks, or nonconsecutive page-word indices.
- Run the focused host test and static checks only; defer simulator/X3 builds to the batch checkpoint.
- Do not commit, flash, push, merge, or modify hardware.

---

### Task 1: Test and add the pure highlight-gap policy

**Files:**
- Create: `src/clippings/ClippingHighlightGeometry.h`
- Create: `test/clipping_highlight_geometry/ClippingHighlightGeometryTest.cpp`
- Create: `test/clipping_highlight_geometry/CMakeLists.txt`
- Modify: `test/CMakeLists.txt`

**Interfaces:**
- Produces: `ClippingHighlightGeometry::WordRect`, `GapRect`, and `gapBetweenAdjacentWords(const WordRect&, const WordRect&, GapRect&)`.
- Consumes: plain signed integer coordinates and a `uint16_t` page-word index; no renderer, page, font, or heap state.

- [ ] **Step 1: Write the failing geometry test**

The test uses literal, hand-checked geometry. It must require an 8-pixel gap from `x=132` to `x=140` in both LTR and RTL word order, and reject different lines, nonconsecutive word indices, overlaps, and invalid rectangles.

- [ ] **Step 2: Compile and verify RED**

Run strict WSL `g++` against only `ClippingHighlightGeometryTest.cpp`. Expected: compilation fails because `src/clippings/ClippingHighlightGeometry.h` does not exist.

- [ ] **Step 3: Add the minimal constexpr helper**

Use fixed-size aggregate rectangles. Return false unless `current.pageWordIndex == previous.pageWordIndex + 1`, `current.y == previous.y`, and both rectangles have positive dimensions. Compute the visual left and right rectangles independently from selection order so RTL works; return exactly their positive intervening rectangle and reject overlap.

- [ ] **Step 4: Compile and verify GREEN**

Run the same strict compile and execute the result. Expected: both exit zero with no warnings.

### Task 2: Integrate the helper into EPUB clipping drawing

**Files:**
- Modify: `src/activities/reader/EpubReaderActivity.cpp:43-60`
- Modify: `src/activities/reader/EpubReaderActivity.cpp:6078-6120`

**Interfaces:**
- Consumes: the helper from Task 1 and the existing selected visible-word callback.
- Produces: one light-gray gap rectangle before the current selected word when the helper approves it.

- [ ] **Step 1: Include the geometry helper**

Add `clippings/ClippingHighlightGeometry.h` with the existing clipping includes.

- [ ] **Step 2: Track the prior selected visible word without allocation**

Keep one `WordRect`, the prior `TextBlock*`, and one boolean outside the callback. Reset the boolean whenever a visible word is not highlighted so an unselected word cannot be bridged.

- [ ] **Step 3: Fill approved gaps**

After calculating the current selected word rectangle, call `gapBetweenAdjacentWords` only when the prior selected word belongs to the same `TextBlock`. If it succeeds, draw `gap` with the existing `Color::LightGray` dither. Then store the current rectangle and block for the next callback. Preserve the existing per-word width extension and black text redraw.

### Task 3: Document and verify the slice

**Files:**
- Modify: `CHANGELOG.md:3-15`

**Interfaces:**
- Produces: one human-facing Unreleased fix entry.

- [ ] **Step 1: Update the changelog**

Document that saved EPUB clipping markers remain continuous through hidden layout spacing and ellipses.

- [ ] **Step 2: Run focused verification**

Run the strict host test, `git diff --check`, and a source audit confirming the helper is allocation-free and only the intended reader path consumes it. Do not run PlatformIO.

- [ ] **Step 3: Record later hardware verification**

On the physical X3, open an EPUB containing an ellipsis or hidden layout spacing, save a clipping spanning it, leave and reopen the book, and confirm the gray marker is continuous on one line but does not connect across line breaks or unselected words. Clear the affected EPUB cache if testing against stale layout output.
