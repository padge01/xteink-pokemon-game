# RTL Width Scratch Safety Specification

## Goal

Adapt CrossPoint development commit `b1f3e91d` so EPUB bidirectional line layout returns a recoverable failure when reordered word-width scratch cannot be allocated on an ESP32-C3.

## Required behavior

- When visual reordering is required, copy `lineWordWidths` into visual order using a local `ArenaVector<uint16_t>` backed by the existing `scratchArena`.
- Preserve the exact width order selected by `visualOrderScratch` and all downstream layout calculations.
- Reserve the complete `visualOrderScratch.size()` capacity before the copy loop.
- Distinguish and report reserve failure from append failure; `ParsedText::extractLine()` must log the failure and return `false`.
- Remove the persistent `reorderedWidthsScratch` `std::vector<uint16_t>` member and its clear, reserve, and swap operations.
- Leave every other reordered scratch container and EPUB layout behavior unchanged.

## Resource constraints

- Each reordered line adds exactly `2 * visualOrderScratch.size()` bytes because each width is `uint16_t`; total width scratch retained by the operation-scoped arena is two bytes per reordered word processed by that layout operation.
- Runtime size can exceed the 256-byte stack guideline, so it must use the existing fallible scratch arena rather than stack storage.
- Each buffer is referenced only by its `extractLine()` call and all width scratch is reclaimed when the local `layoutArena` is destroyed at the end of the layout operation; it adds no `ParsedText` member capacity.
- Add no setting, translation, dependency, task, framebuffer, cache-format, or PSRAM assumption.
- Do not run PlatformIO, flash hardware, commit, push, or merge in this slice.

## Verification

- A strict C++20 host regression must prove literal reordered output plus reserve- and append-failure results.
- `git diff --check` must report no whitespace errors.
- Later batched X3 verification: open an RTL or mixed-direction EPUB after clearing its `.crosspoint/epub_<hash>/` cache, page through affected text, and confirm correct ordering; memory-pressure serial logs must show `PTX` OOM and a safe layout failure rather than a restart if the scratch allocation cannot be satisfied.
