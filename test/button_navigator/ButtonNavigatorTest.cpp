#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <new>

#include "util/ButtonNavigator.h"

namespace {

std::atomic<size_t> allocationCount = 0;
int callbackCount = 0;
uint32_t currentTimeMs = 0;

void countCallback() { callbackCount++; }

bool singleButtonReleaseDoesNotAlsoMatchBack() {
  MappedInputManager input;
  ButtonNavigator navigator;
  callbackCount = 0;
  ButtonNavigator::setMappedInputManager(input);
  input.setReleased(MappedInputManager::Button::Back, true);

  navigator.onRelease({MappedInputManager::Button::Left}, countCallback);

  return callbackCount == 0;
}

bool nextReleaseMatchesEitherMappedButton() {
  MappedInputManager input;
  ButtonNavigator navigator;
  callbackCount = 0;
  ButtonNavigator::setMappedInputManager(input);
  input.setReleased(MappedInputManager::Button::Right, true);

  navigator.onNextRelease(countCallback);

  return callbackCount == 1;
}

bool nextReleaseDoesNotAllocateForItsButtonList() {
  MappedInputManager input;
  ButtonNavigator navigator;
  callbackCount = 0;
  ButtonNavigator::setMappedInputManager(input);
  const std::function<void()> callback = countCallback;
  const size_t allocationsBefore = allocationCount.load();

  navigator.onNextRelease(callback);

  return allocationCount.load() == allocationsBefore;
}

bool pressNavigationAcceptsDirectionalAndFrontButtons() {
  for (const auto button : {MappedInputManager::Button::Down, MappedInputManager::Button::Right}) {
    MappedInputManager input;
    ButtonNavigator navigator;
    callbackCount = 0;
    ButtonNavigator::setMappedInputManager(input);
    input.setPressed(button, true);
    navigator.onNextPress(countCallback);
    if (callbackCount != 1) return false;
  }

  for (const auto button : {MappedInputManager::Button::Up, MappedInputManager::Button::Left}) {
    MappedInputManager input;
    ButtonNavigator navigator;
    callbackCount = 0;
    ButtonNavigator::setMappedInputManager(input);
    input.setPressed(button, true);
    navigator.onPreviousPress(countCallback);
    if (callbackCount != 1) return false;
  }
  return true;
}

bool indexNavigationWrapsTheFullPokedex() {
  return ButtonNavigator::nextIndex(150, 151) == 0 && ButtonNavigator::previousIndex(0, 151) == 150 &&
         ButtonNavigator::nextIndex(4, 151) == 5;
}

}  // namespace

uint32_t millis() { return currentTimeMs; }

void* operator new(const size_t size) {
  allocationCount++;
  if (void* memory = std::malloc(size)) return memory;
  throw std::bad_alloc();
}

void* operator new[](const size_t size) {
  allocationCount++;
  if (void* memory = std::malloc(size)) return memory;
  throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, size_t) noexcept { std::free(memory); }

int main() {
  if (!singleButtonReleaseDoesNotAlsoMatchBack()) {
    std::fputs("single-button release also matched Back\n", stderr);
    return 1;
  }
  if (!nextReleaseMatchesEitherMappedButton()) {
    std::fputs("next release did not match Right\n", stderr);
    return 2;
  }
  if (!nextReleaseDoesNotAllocateForItsButtonList()) {
    std::fputs("next release allocated its button list\n", stderr);
    return 3;
  }
  if (!pressNavigationAcceptsDirectionalAndFrontButtons()) {
    std::fputs("press navigation did not accept both directional button pairs\n", stderr);
    return 4;
  }
  if (!indexNavigationWrapsTheFullPokedex()) {
    std::fputs("index navigation did not traverse and wrap the full Pokedex\n", stderr);
    return 5;
  }
  return 0;
}
