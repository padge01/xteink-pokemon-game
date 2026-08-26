#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

uint32_t millis();

class MappedInputManager {
 public:
  enum class Button { Back, Confirm, Left, Right, Up, Down, Power, PageBack, PageForward };
  static constexpr size_t BUTTON_COUNT = static_cast<size_t>(Button::PageForward) + 1;

  bool wasPressed(const Button button) const { return pressed_[index(button)]; }
  bool wasReleased(const Button button) const { return released_[index(button)]; }
  bool isPressed(const Button button) const { return held_[index(button)]; }
  uint32_t getHeldTime() const { return heldTimeMs_; }

  void setPressed(const Button button, const bool value) { pressed_[index(button)] = value; }
  void setReleased(const Button button, const bool value) { released_[index(button)] = value; }
  void setHeld(const Button button, const bool value) { held_[index(button)] = value; }
  void setHeldTime(const uint32_t value) { heldTimeMs_ = value; }

 private:
  static constexpr size_t index(const Button button) { return static_cast<size_t>(button); }

  std::array<bool, BUTTON_COUNT> pressed_{};
  std::array<bool, BUTTON_COUNT> released_{};
  std::array<bool, BUTTON_COUNT> held_{};
  uint32_t heldTimeMs_ = 0;
};
