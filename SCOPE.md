# Scope

Xteink Pokémon Game is a lightweight reading companion for the Xteink X3. It extends CrossInk without replacing its main job as an e-reader.

## In scope

- Award progress from CrossInk's existing verified reading sessions
- One lead Pokémon, a Party of six, PC storage, items, evolution, and the original-151 Pokédex
- Small, offline menus designed for the X3's buttons, e-ink display, and limited memory
- Preprocessed artwork streamed from the SD card
- Save files kept separate from books and reading statistics
- X3 stability fixes and compatible upstream CrossPoint/CrossInk improvements

## Out of scope

- Battles, moves, combat statistics, breeding, or online trading
- Background networking, timers, or work while the device is idle
- A second framebuffer or large in-memory artwork
- Features that duplicate CrossInk's reader, reading-session, library, or file-transfer systems
- X4, X4 Pro, Sticky, or other device support until each target has its own design and hardware validation

## Gate for new work

A change should reuse an existing CrossInk mechanism, remain safe on the ESP32-C3, and improve the reading companion directly. New screens, settings, dependencies, and persistent fields need a concrete user benefit and hardware validation.
