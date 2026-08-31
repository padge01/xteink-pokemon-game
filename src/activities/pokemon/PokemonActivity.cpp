#if defined(CROSSINK_ENABLE_POKEMON)

#include "PokemonActivity.h"

#include <I18n.h>
#include <Logging.h>
#include <Memory.h>
#include <PokemonSpecies.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

#include "activities/util/KeyboardEntryActivity.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "components/UIThemeTokens.h"
#include "components/UiAppHelpers.h"
#include "components/pokemon/PokemonArt.h"
#include "fontIds.h"

namespace fui = freeink::ui;

namespace {
constexpr fui::ActionId ACTION_ROW = 1;
constexpr uint16_t STARTERS[] = {1, 4, 7, 25};

const char* speciesName(const uint16_t id) {
  const pokemon::SpeciesData* species = pokemon::speciesData(id);
  return species == nullptr ? "???" : species->name;
}

const char* genderText(const pokemon::Gender gender) {
  if (gender == pokemon::Gender::Male) return tr(STR_POKEMON_MALE);
  if (gender == pokemon::Gender::Female) return tr(STR_POKEMON_FEMALE);
  return "";
}

const char* itemName(const pokemon::EvolutionItem item) {
  switch (item) {
    case pokemon::EvolutionItem::MoonStone:
      return tr(STR_POKEMON_MOON_STONE);
    case pokemon::EvolutionItem::FireStone:
      return tr(STR_POKEMON_FIRE_STONE);
    case pokemon::EvolutionItem::ThunderStone:
      return tr(STR_POKEMON_THUNDER_STONE);
    case pokemon::EvolutionItem::WaterStone:
      return tr(STR_POKEMON_WATER_STONE);
    case pokemon::EvolutionItem::LeafStone:
      return tr(STR_POKEMON_LEAF_STONE);
    case pokemon::EvolutionItem::LinkCable:
      return tr(STR_POKEMON_LINK_CABLE);
    default:
      return "";
  }
}

void centered(const GfxRenderer& renderer, const int font, const int y, const char* text,
              const EpdFontFamily::Style style = EpdFontFamily::REGULAR) {
  if (const char* newline = strchr(text, '\n')) {
    char first[64];
    snprintf(first, sizeof(first), "%.*s", static_cast<int>(newline - text), text);
    centered(renderer, font, y, first, style);
    centered(renderer, font, y + 28, newline + 1, style);
    return;
  }
  renderer.drawText(font, (renderer.getScreenWidth() - renderer.getTextWidth(font, text, style)) / 2, y, text, true,
                    style);
}
}  // namespace

PokemonActivity::PokemonActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("Pokemon", renderer, mappedInput),
      service_(pokemon::devicePokemonService()),
      uiTarget_(makeUiTarget(renderer)),
      app_(uiTarget_, uiTarget_.deviceContext()) {}

void PokemonActivity::onEnter() {
  Activity::onEnter();
  app_.setTheme(uiThemeTokens(uiTarget_));
  app_.on(ACTION_ROW, &PokemonActivity::onRow, this);
  loadInitialScreen();
}

void PokemonActivity::loadInitialScreen() {
  const pokemon::ServiceStatus status = service_.loadSnapshot(snapshot_);
  if (status == pokemon::ServiceStatus::Empty) {
    setScreen(Screen::Starter);
  } else if (status != pokemon::ServiceStatus::Ok) {
    showMessage(tr(STR_POKEMON_SAVE_ERROR), Screen::Menu);
  } else {
    setScreen(snapshot_.state.pending.kind == pokemon::PendingEventKind::None ? Screen::Menu : Screen::Event);
  }
}

bool PokemonActivity::refreshSnapshot() {
  if (service_.loadSnapshot(snapshot_) != pokemon::ServiceStatus::Ok) {
    showMessage(tr(STR_POKEMON_SAVE_ERROR), Screen::Menu);
    return false;
  }
  return true;
}

void PokemonActivity::setScreen(const Screen screen, const int selected) {
  screen_ = screen;
  selected_ = std::max(0, selected);
  uiReady_ = false;
  app_.setScreen(&PokemonActivity::screenBuilder, this);
  requestUpdate();
}

bool PokemonActivity::isListScreen() const {
  return screen_ != Screen::Summary && screen_ != Screen::Message;
}

int PokemonActivity::logicalCount() const {
  switch (screen_) {
    case Screen::Starter:
      return 4;
    case Screen::Gender:
    case Screen::NicknameQuestion:
    case Screen::Move:
    case Screen::ResetFirst:
    case Screen::ResetFinal:
      return screen_ == Screen::Move ? snapshot_.partyCount : 2;
    case Screen::Menu:
      return 6;
    case Screen::Party:
      return pokemon::PARTY_SIZE;
    case Screen::Actions: {
      const auto actions =
          pokemon::collectionActions(actionSource_ == Screen::Party, snapshot_.partyCount);
      return actions.count;
    }
    case Screen::Pc:
      return static_cast<int>(snapshot_.ownedCount - snapshot_.partyCount);
    case Screen::PcOrder:
      return 3;
    case Screen::Bag:
      return pokemon::EVOLUTION_ITEM_COUNT;
    case Screen::ItemTarget:
      return snapshot_.partyCount;
    case Screen::Pokedex:
      return pokemon::KANTO_SPECIES_COUNT;
    case Screen::Event:
      return snapshot_.state.pending.kind == pokemon::PendingEventKind::Item ? 1 : 2;
    case Screen::Summary:
    case Screen::Message:
      return 0;
  }
  return 0;
}

int PokemonActivity::pageStart() const { return (selected_ / 5) * 5; }

uint32_t PokemonActivity::selectedRecordId() const {
  if (screen_ == Screen::Party || screen_ == Screen::Move || screen_ == Screen::ItemTarget) {
    return selected_ < snapshot_.partyCount ? snapshot_.party[selected_].recordId : 0;
  }
  if (screen_ == Screen::Pc) {
    const int local = selected_ - pageStart();
    return local >= 0 && local < static_cast<int>(pcCount_) ? pcPage_[local].recordId : 0;
  }
  return focusedRecordId_;
}

void PokemonActivity::showMessage(const char* message, const Screen returnScreen) {
  snprintf(message_, sizeof(message_), "%s", message == nullptr ? "" : message);
  returnScreen_ = returnScreen;
  setScreen(Screen::Message);
}

void PokemonActivity::finishStarter(const char* nickname) {
  const auto status = service_.createStarter(starterSpecies_, starterGender_, nickname == nullptr ? "" : nickname);
  if (status != pokemon::ServiceStatus::Ok) {
    showMessage(tr(STR_POKEMON_SAVE_ERROR), Screen::Starter);
    return;
  }
  if (!refreshSnapshot()) return;
  setScreen(Screen::Menu);
}

void PokemonActivity::openNickname(const uint32_t recordId, const bool starter) {
  auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_POKEMON_NICKNAME), "", 32,
                                                            InputType::Text);
  if (!keyboard) {
    LOG_ERR("PokemonActivity", "Could not allocate nickname keyboard");
    showMessage(tr(STR_POKEMON_SAVE_ERROR), starter ? Screen::Starter : Screen::Menu);
    return;
  }
  startActivityForResult(std::move(keyboard), [this, recordId, starter](const ActivityResult& result) {
    if (result.isCancelled) {
      setScreen(starter || recordId == nicknameRecordId_ ? Screen::NicknameQuestion : Screen::Actions);
      return;
    }
    const auto* keyboardResult = std::get_if<KeyboardResult>(&result.data);
    if (keyboardResult == nullptr) return;
    if (starter) {
      finishStarter(keyboardResult->text.c_str());
    } else if (service_.renamePokemon(recordId, keyboardResult->text) == pokemon::ServiceStatus::Ok) {
      if (!refreshSnapshot()) return;
      nicknameRecordId_ = 0;
      setScreen(Screen::Menu);
    } else {
      showMessage(tr(STR_POKEMON_SAVE_ERROR), Screen::Menu);
    }
  });
}

void PokemonActivity::activate() {
  switch (screen_) {
    case Screen::Starter:
      starterSpecies_ = STARTERS[selected_];
      setScreen(Screen::Gender);
      return;
    case Screen::Gender:
      starterGender_ = selected_ == 0 ? pokemon::Gender::Male : pokemon::Gender::Female;
      nicknameRecordId_ = 0;
      snprintf(message_, sizeof(message_), tr(STR_POKEMON_NICKNAME_QUESTION), speciesName(starterSpecies_));
      setScreen(Screen::NicknameQuestion);
      return;
    case Screen::NicknameQuestion:
      if (selected_ == 0) openNickname(nicknameRecordId_, nicknameRecordId_ == 0);
      else if (nicknameRecordId_ == 0) finishStarter("");
      else {
        nicknameRecordId_ = 0;
        setScreen(Screen::Menu);
      }
      return;
    case Screen::Menu:
      if (selected_ == 0) setScreen(Screen::Party);
      else if (selected_ == 1) setScreen(Screen::Pc);
      else if (selected_ == 2) setScreen(Screen::Bag);
      else if (selected_ == 3) setScreen(Screen::Pokedex);
      else if (selected_ == 4) setScreen(Screen::PcOrder, static_cast<int>(pcOrder_));
      else setScreen(Screen::ResetFirst);
      return;
    case Screen::Party:
    case Screen::Pc: {
      const uint32_t recordId = selectedRecordId();
      if (recordId == 0) return;
      focusedRecordId_ = recordId;
      actionSource_ = screen_;
      setScreen(Screen::Summary);
      return;
    }
    case Screen::Summary:
      setScreen(Screen::Actions);
      return;
    case Screen::Actions: {
      const bool party = actionSource_ == Screen::Party;
      const auto actions = pokemon::collectionActions(party, snapshot_.partyCount);
      if (selected_ < 0 || selected_ >= actions.count) return;
      switch (actions.items[selected_]) {
        case pokemon::CollectionAction::Summary:
          setScreen(Screen::Summary);
          return;
        case pokemon::CollectionAction::Move: {
          int slot = 0;
          while (slot < snapshot_.partyCount && snapshot_.party[slot].recordId != focusedRecordId_) ++slot;
          setScreen(Screen::Move, slot);
          return;
        }
        case pokemon::CollectionAction::Deposit:
        case pokemon::CollectionAction::Withdraw: {
          const auto status = party ? service_.depositPokemon(focusedRecordId_)
                                    : service_.withdrawPokemon(focusedRecordId_);
          if (status == pokemon::ServiceStatus::LastPokemon)
            showMessage(tr(STR_POKEMON_LAST_PARTY), actionSource_);
          else if (status == pokemon::ServiceStatus::PartyFull)
            showMessage(tr(STR_POKEMON_PARTY_FULL), actionSource_);
          else if (status != pokemon::ServiceStatus::Ok)
            showMessage(tr(STR_POKEMON_SAVE_ERROR), actionSource_);
          else {
            if (!refreshSnapshot()) return;
            setScreen(actionSource_);
          }
          return;
        }
        case pokemon::CollectionAction::Rename:
          openNickname(focusedRecordId_, false);
          return;
        case pokemon::CollectionAction::EvolutionPrompts: {
          pokemon::PokemonRecord record{};
          if (service_.readRecord(focusedRecordId_, record) != pokemon::ServiceStatus::Ok) return;
          const bool enabled =
              (record.flags & pokemon::recordFlag(pokemon::RecordFlag::EvolutionPromptsDisabled)) == 0;
          if (service_.setEvolutionPrompts(focusedRecordId_, !enabled) != pokemon::ServiceStatus::Ok) {
            showMessage(tr(STR_POKEMON_SAVE_ERROR), Screen::Actions);
          } else {
            if (!refreshSnapshot()) return;
            setScreen(Screen::Summary);
          }
          return;
        }
      }
      break;
    }
    case Screen::Move: {
      int from = 0;
      while (from < snapshot_.partyCount && snapshot_.party[from].recordId != focusedRecordId_) ++from;
      const auto status = service_.movePartyMember(static_cast<uint8_t>(from), static_cast<uint8_t>(selected_));
      if (status != pokemon::ServiceStatus::Ok) showMessage(tr(STR_POKEMON_SAVE_ERROR), Screen::Party);
      else {
        if (!refreshSnapshot()) return;
        setScreen(Screen::Party);
      }
      return;
    }
    case Screen::PcOrder:
      pcOrder_ = static_cast<pokemon::PcOrder>(selected_);
      setScreen(Screen::Pc);
      return;
    case Screen::Bag:
      selectedItem_ = static_cast<pokemon::EvolutionItem>(selected_ + 1);
      if (snapshot_.state.itemCounts[selected_] == 0) showMessage(tr(STR_POKEMON_NOT_APPLICABLE), Screen::Bag);
      else setScreen(Screen::ItemTarget);
      return;
    case Screen::ItemTarget: {
      const auto status = service_.useEvolutionItem(selectedRecordId(), selectedItem_);
      if (status == pokemon::ServiceStatus::NotApplicable) showMessage(tr(STR_POKEMON_NOT_APPLICABLE), Screen::Bag);
      else if (status != pokemon::ServiceStatus::Ok) showMessage(tr(STR_POKEMON_SAVE_ERROR), Screen::Bag);
      else {
        if (!refreshSnapshot()) return;
        setScreen(Screen::Party);
      }
      return;
    }
    case Screen::Pokedex:
      return;
    case Screen::Event: {
      const pokemon::PendingEvent pending = snapshot_.state.pending;
      if (pending.kind == pokemon::PendingEventKind::Encounter) {
        uint32_t caught = 0;
        const auto choice = selected_ == 0 ? pokemon::EncounterChoice::Catch : pokemon::EncounterChoice::Pass;
        if (service_.resolveEncounter(choice, caught) != pokemon::ServiceStatus::Ok) {
          showMessage(tr(STR_POKEMON_SAVE_ERROR), Screen::Event);
          return;
        }
        if (!refreshSnapshot()) return;
        if (caught != 0) {
          nicknameRecordId_ = caught;
          snprintf(message_, sizeof(message_), tr(STR_POKEMON_NICKNAME_QUESTION), speciesName(pending.speciesId));
          setScreen(Screen::NicknameQuestion);
        } else setScreen(Screen::Menu);
      } else if (pending.kind == pokemon::PendingEventKind::Item) {
        if (service_.acknowledgeItem() != pokemon::ServiceStatus::Ok) showMessage(tr(STR_POKEMON_SAVE_ERROR), Screen::Event);
        else {
          if (!refreshSnapshot()) return;
          setScreen(Screen::Menu);
        }
      } else if (pending.kind == pokemon::PendingEventKind::Evolution) {
        const auto choice = selected_ == 0 ? pokemon::EvolutionChoice::Evolve : pokemon::EvolutionChoice::Cancel;
        if (service_.resolveEvolution(choice) != pokemon::ServiceStatus::Ok) showMessage(tr(STR_POKEMON_SAVE_ERROR), Screen::Event);
        else {
          if (!refreshSnapshot()) return;
          setScreen(Screen::Menu);
        }
      }
      return;
    }
    case Screen::ResetFirst:
      if (selected_ == 0) setScreen(Screen::ResetFinal);
      else setScreen(Screen::Menu);
      return;
    case Screen::ResetFinal:
      if (selected_ == 0 && service_.reset() == pokemon::ServiceStatus::Ok) {
        snapshot_ = {};
        setScreen(Screen::Starter);
      } else if (selected_ == 0) showMessage(tr(STR_POKEMON_SAVE_ERROR), Screen::Menu);
      else setScreen(Screen::Menu);
      return;
    case Screen::Message:
      setScreen(returnScreen_);
      return;
  }
}

void PokemonActivity::goBack() {
  switch (screen_) {
    case Screen::Starter:
    case Screen::Menu:
      finishAfterBackPress();
      return;
    case Screen::Gender:
      setScreen(Screen::Starter);
      return;
    case Screen::NicknameQuestion:
      if (nicknameRecordId_ == 0) setScreen(Screen::Gender);
      else setScreen(Screen::Menu);
      return;
    case Screen::Summary:
    case Screen::Actions:
      setScreen(actionSource_);
      return;
    case Screen::Move:
      setScreen(Screen::Party);
      return;
    case Screen::PcOrder:
      setScreen(Screen::Pc);
      return;
    case Screen::ItemTarget:
      setScreen(Screen::Bag);
      return;
    case Screen::Message:
      setScreen(returnScreen_);
      return;
    default:
      setScreen(Screen::Menu);
      return;
  }
}

void PokemonActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer) ||
      mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    goBack();
    return;
  }
  if (uiReady_) {
    const auto snap = touchSnapshotFrom(mappedInput);
    if (snap.touchPressed || snap.touchReleased) {
      const auto event = app_.route(snap);
      if (app_.invalidated()) requestUpdate();
      if (event) return;
    }
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    activate();
    return;
  }
  const int count = logicalCount();
  if (count <= 0) return;
  const auto move = [this, count](const int next) {
    selected_ = next;
    requestUpdate();
  };
  navigator_.onNextRelease([this, count, &move] { move(ButtonNavigator::nextIndex(selected_, count)); });
  navigator_.onPreviousRelease([this, count, &move] { move(ButtonNavigator::previousIndex(selected_, count)); });
  navigator_.onNextContinuous([this, count, &move] { move(ButtonNavigator::nextPageIndex(selected_, count, 5)); });
  navigator_.onPreviousContinuous([this, count, &move] { move(ButtonNavigator::previousPageIndex(selected_, count, 5)); });
}

void PokemonActivity::screenBuilder(UiApp::ScreenType& screen, void* user) {
  static_cast<PokemonActivity*>(user)->buildUi(screen);
}

void PokemonActivity::onRow(const fui::ActionEvent& event, void* user) {
  auto* self = static_cast<PokemonActivity*>(user);
  if (event.value < 0 || event.value >= self->logicalCount()) return;
  self->selected_ = event.value;
  self->app_.clearTapFlash();
  self->activate();
}

void PokemonActivity::buildRows() {
  for (size_t i = 0; i < rows_.size(); ++i) {
    rows_[i] = {};
    labels_[i].fill('\0');
    values_[i].fill('\0');
  }
  const int start = pageStart();
  const int total = logicalCount();
  rowCount_ = std::min(5, total - start);
  const auto row = [this, start](const int local, const char* label, const char* value = nullptr) {
    snprintf(labels_[local].data(), labels_[local].size(), "%s", label == nullptr ? "" : label);
    if (value != nullptr) snprintf(values_[local].data(), values_[local].size(), "%s", value);
    rows_[local].label = labels_[local].data();
    rows_[local].value = value == nullptr ? nullptr : values_[local].data();
    rows_[local].actionValue = static_cast<int16_t>(start + local);
  };

  for (int local = 0; local < rowCount_; ++local) {
    const int index = start + local;
    switch (screen_) {
      case Screen::Starter:
        row(local, speciesName(STARTERS[index]));
        break;
      case Screen::Gender:
        row(local, index == 0 ? tr(STR_POKEMON_MALE) : tr(STR_POKEMON_FEMALE));
        break;
      case Screen::NicknameQuestion:
      case Screen::ResetFirst:
      case Screen::ResetFinal:
        row(local, index == 0 ? tr(STR_YES) : tr(STR_NO));
        break;
      case Screen::Menu:
        row(local, index == 0 ? tr(STR_POKEMON_PARTY)
                              : index == 1 ? tr(STR_POKEMON_PC_BOX)
                              : index == 2 ? tr(STR_POKEMON_BAG)
                              : index == 3 ? tr(STR_POKEDEX)
                              : index == 4 ? tr(STR_POKEMON_PC_SORT)
                                           : tr(STR_POKEMON_RESET));
        break;
      case Screen::Party:
      case Screen::Move:
      case Screen::ItemTarget: {
        if (index >= snapshot_.partyCount) {
          row(local, tr(STR_POKEMON_EMPTY));
          break;
        }
        const auto& record = snapshot_.party[index];
        char value[24];
        snprintf(value, sizeof(value), "%s %u  %s", tr(STR_POKEMON_LEVEL), pokemon::levelForXp(record.totalXp),
                 genderText(record.gender));
        row(local, record.nickname[0] == '\0' ? speciesName(record.speciesId) : record.nickname.data(), value);
        break;
      }
      case Screen::Actions: {
        const auto actions =
            pokemon::collectionActions(actionSource_ == Screen::Party, snapshot_.partyCount);
        if (index >= actions.count) break;
        const char* label = nullptr;
        switch (actions.items[index]) {
          case pokemon::CollectionAction::Summary:
            label = tr(STR_POKEMON_SUMMARY);
            break;
          case pokemon::CollectionAction::Move:
            label = tr(STR_POKEMON_MOVE);
            break;
          case pokemon::CollectionAction::Deposit:
            label = tr(STR_POKEMON_DEPOSIT);
            break;
          case pokemon::CollectionAction::Withdraw:
            label = tr(STR_POKEMON_WITHDRAW);
            break;
          case pokemon::CollectionAction::Rename:
            label = tr(STR_POKEMON_RENAME);
            break;
          case pokemon::CollectionAction::EvolutionPrompts:
            label = tr(STR_POKEMON_EVOLUTIONS);
            break;
        }
        row(local, label);
        break;
      }
      case Screen::Pc: {
        if (local == 0) {
          pcCount_ = 0;
          if (service_.readPcPage(pcOrder_, start, pcPage_, pcCount_) != pokemon::ServiceStatus::Ok) {
            rowCount_ = 1;
            row(local, tr(STR_POKEMON_LOAD_ERROR));
            break;
          }
          rowCount_ = static_cast<int>(pcCount_);
          if (rowCount_ == 0) break;
        }
        if (local >= static_cast<int>(pcCount_)) break;
        const auto& record = pcPage_[local];
        char value[24];
        snprintf(value, sizeof(value), "%s %u  %s", tr(STR_POKEMON_LEVEL), pokemon::levelForXp(record.totalXp),
                 genderText(record.gender));
        row(local, record.nickname[0] == '\0' ? speciesName(record.speciesId) : record.nickname.data(), value);
        break;
      }
      case Screen::PcOrder:
        row(local, index == 0 ? tr(STR_POKEMON_CATCH_DATE)
                              : index == 1 ? tr(STR_POKEMON_NUMBER) : tr(STR_POKEMON_ALPHABETICAL));
        break;
      case Screen::Bag: {
        const auto item = static_cast<pokemon::EvolutionItem>(index + 1);
        char count[16];
        snprintf(count, sizeof(count), "× %u", snapshot_.state.itemCounts[index]);
        row(local, itemName(item), count);
        break;
      }
      case Screen::Pokedex: {
        const uint16_t speciesId = static_cast<uint16_t>(index + 1);
        const bool caught = pokemon::isSpeciesMarked(snapshot_.state.caughtSpecies, speciesId);
        const bool seen = pokemon::isSpeciesMarked(snapshot_.state.seenSpecies, speciesId);
        char label[40];
        snprintf(label, sizeof(label), "No. %03u  %s", speciesId, seen ? speciesName(speciesId) : "???");
        row(local, label, caught ? tr(STR_POKEMON_CAUGHT) : seen ? tr(STR_POKEMON_SEEN) : nullptr);
        break;
      }
      case Screen::Event:
        if (snapshot_.state.pending.kind == pokemon::PendingEventKind::Encounter) {
          row(local, index == 0 ? tr(STR_POKEMON_CATCH) : tr(STR_POKEMON_PASS));
        } else if (snapshot_.state.pending.kind == pokemon::PendingEventKind::Evolution) {
          row(local, index == 0 ? tr(STR_POKEMON_EVOLVE) : tr(STR_POKEMON_CANCEL));
        } else row(local, tr(STR_OK));
        break;
      case Screen::Summary:
      case Screen::Message:
        break;
    }
  }
}

void PokemonActivity::buildList(UiApp::ScreenType& screen) {
  buildRows();
  const auto& metrics = UITheme::getInstance().getMetrics();
  int top = metrics.topPadding + TouchHeaderBackButton::height(metrics, mappedInput) + metrics.verticalSpacing;
  rowHeight_ = 64;
  if (screen_ == Screen::Starter || screen_ == Screen::ResetFirst || screen_ == Screen::ResetFinal) top += 72;
  if (screen_ == Screen::Gender) top += 180;
  if (screen_ == Screen::NicknameQuestion) top += 210;
  if (screen_ == Screen::Event) top = renderer.getScreenHeight() - metrics.buttonHintsHeight - rowCount_ * rowHeight_ - 8;
  listBounds_ = Rect{8, top, renderer.getScreenWidth() - 16, rowCount_ * rowHeight_};
  screen.setContentMargin(fui::Insets{static_cast<int16_t>(listBounds_.y), 8,
                                      static_cast<int16_t>(renderer.getScreenHeight() - listBounds_.y - listBounds_.height), 8});
  fui::ListProps props;
  props.items = rows_.data();
  props.count = static_cast<uint16_t>(std::max(0, rowCount_));
  props.selectedIndex = static_cast<int16_t>(selected_ - pageStart());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.rowHeight = static_cast<int16_t>(rowHeight_);
  props.rowGap = 0;
  props.sidePadding = (screen_ == Screen::Party || screen_ == Screen::Move || screen_ == Screen::Pc ||
                       screen_ == Screen::Bag || screen_ == Screen::ItemTarget || screen_ == Screen::Pokedex ||
                       screen_ == Screen::Starter)
                          ? 94
                          : 16;
  props.valueInset = 8;
  screen.list(props);
}

void PokemonActivity::buildUi(UiApp::ScreenType& screen) {
  if (isListScreen()) buildList(screen);
}

void PokemonActivity::renderFocused() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int contentTop = metrics.topPadding + TouchHeaderBackButton::height(metrics, mappedInput) + 14;
  if (screen_ == Screen::Message) {
    centered(renderer, UI_12_FONT_ID, contentTop + 100, message_, EpdFontFamily::BOLD);
    return;
  }
  if (screen_ == Screen::Starter) {
    centered(renderer, UI_12_FONT_ID, contentTop + 18, tr(STR_POKEMON_CHOOSE_PARTNER), EpdFontFamily::BOLD);
    return;
  }
  if (screen_ == Screen::Gender) {
    centered(renderer, UI_12_FONT_ID, contentTop + 18, tr(STR_POKEMON_CHOOSE_GENDER), EpdFontFamily::BOLD);
    pokemon::drawPokemonSpeciesArt(renderer, starterSpecies_, true,
                                   Rect{(renderer.getScreenWidth() - 120) / 2, contentTop + 54, 120, 90});
    return;
  }
  if (screen_ == Screen::Pc && logicalCount() == 0) {
    centered(renderer, UI_12_FONT_ID, contentTop + 100, tr(STR_POKEMON_NO_STORED), EpdFontFamily::BOLD);
    return;
  }
  if (screen_ == Screen::NicknameQuestion || screen_ == Screen::ResetFirst || screen_ == Screen::ResetFinal) {
    const char* prompt = screen_ == Screen::NicknameQuestion
                             ? message_
                             : screen_ == Screen::ResetFirst ? tr(STR_POKEMON_RESET_QUESTION) : tr(STR_POKEMON_RESET_CONFIRM);
    centered(renderer, UI_12_FONT_ID, contentTop + 18, prompt, EpdFontFamily::BOLD);
    if (screen_ == Screen::NicknameQuestion) {
      pokemon::drawPokemonSpeciesArt(renderer, starterSpecies_, true,
                                     Rect{(renderer.getScreenWidth() - 120) / 2, contentTop + 82, 120, 90});
    }
    return;
  }
  if (screen_ == Screen::Summary) {
    pokemon::PokemonRecord record{};
    if (service_.readRecord(focusedRecordId_, record) != pokemon::ServiceStatus::Ok) return;
    const bool landscape = renderer.getScreenWidth() > renderer.getScreenHeight();
    const int artX = landscape ? 28 : (renderer.getScreenWidth() - 120) / 2;
    const int artY = contentTop + 8;
    pokemon::drawPokemonSpeciesArt(renderer, record.speciesId, true, Rect{artX, artY, 120, 90});
    const int textX = landscape ? 178 : 28;
    int y = landscape ? contentTop + 8 : artY + 112;
    renderer.drawText(UI_12_FONT_ID, textX, y, speciesName(record.speciesId), true, EpdFontFamily::BOLD);
    y += 28;
    renderer.drawText(UI_10_FONT_ID, textX, y, record.nickname.data());
    y += 30;
    char line[96];
    snprintf(line, sizeof(line), "%s %03u    %s %u    %s", tr(STR_POKEMON_NUMBER_SHORT), record.speciesId,
             tr(STR_POKEMON_LEVEL),
             pokemon::levelForXp(record.totalXp), genderText(record.gender));
    renderer.drawText(UI_10_FONT_ID, textX, y, line);
    y += 26;
    snprintf(line, sizeof(line), "%s    %lu", tr(STR_POKEMON_EXP_POINTS), static_cast<unsigned long>(record.totalXp));
    renderer.drawText(UI_10_FONT_ID, textX, y, line);
    y += 26;
    snprintf(line, sizeof(line), "%s    %s %u", tr(STR_POKEMON_MET), tr(STR_POKEMON_LEVEL), record.caughtLevel);
    renderer.drawText(UI_10_FONT_ID, textX, y, line);
    y += 26;
    const uint8_t level = pokemon::levelForXp(record.totalXp);
    const uint32_t next = level >= 100 ? pokemon::MAXIMUM_TOTAL_XP : pokemon::xpRequired(level + 1);
    snprintf(line, sizeof(line), "%s    %lu / %lu", tr(STR_POKEMON_EXP_POINTS), static_cast<unsigned long>(record.totalXp),
             static_cast<unsigned long>(next));
    renderer.drawText(UI_10_FONT_ID, textX, y, line, true, EpdFontFamily::BOLD);
    y += 26;
    const bool prompts = (record.flags & pokemon::recordFlag(pokemon::RecordFlag::EvolutionPromptsDisabled)) == 0;
    snprintf(line, sizeof(line), "%s    %s", tr(STR_POKEMON_EVOLUTIONS), prompts ? tr(STR_POKEMON_ON) : tr(STR_POKEMON_OFF));
    renderer.drawText(UI_10_FONT_ID, textX, y, line);
    return;
  }
  if (screen_ != Screen::Event) return;
  const auto& pending = snapshot_.state.pending;
  char line[96];
  if (pending.kind == pokemon::PendingEventKind::Encounter) {
    pokemon::drawPokemonSpeciesArt(renderer, pending.speciesId, true,
                                   Rect{(renderer.getScreenWidth() - 120) / 2, contentTop + 8, 120, 90});
    snprintf(line, sizeof(line), tr(STR_POKEMON_WILD_APPEARED), speciesName(pending.speciesId));
    centered(renderer, UI_12_FONT_ID, contentTop + 112, line, EpdFontFamily::BOLD);
    snprintf(line, sizeof(line), "%s %u    %s", tr(STR_POKEMON_LEVEL), pending.level, genderText(pending.gender));
    centered(renderer, UI_10_FONT_ID, contentTop + 140, line);
  } else if (pending.kind == pokemon::PendingEventKind::Item) {
    pokemon::drawPokemonItemArt(renderer, pending.item, true,
                                Rect{(renderer.getScreenWidth() - 120) / 2, contentTop + 8, 120, 90}, false);
    snprintf(line, sizeof(line), tr(STR_POKEMON_FOUND_ITEM), itemName(pending.item));
    centered(renderer, UI_12_FONT_ID, contentTop + 112, line, EpdFontFamily::BOLD);
  } else if (pending.kind == pokemon::PendingEventKind::Evolution) {
    pokemon::PokemonRecord record{};
    if (service_.readRecord(pending.recordId, record) != pokemon::ServiceStatus::Ok) return;
    pokemon::drawPokemonSpeciesArt(renderer, record.speciesId, true,
                                   Rect{(renderer.getScreenWidth() - 120) / 2, contentTop + 8, 120, 90});
    snprintf(line, sizeof(line), tr(STR_POKEMON_EVOLVING), speciesName(record.speciesId));
    centered(renderer, UI_12_FONT_ID, contentTop + 112, line, EpdFontFamily::BOLD);
  }
}

void PokemonActivity::renderRowArt() {
  const bool artRows = screen_ == Screen::Starter || screen_ == Screen::Party || screen_ == Screen::Move ||
                       screen_ == Screen::Pc || screen_ == Screen::Bag || screen_ == Screen::ItemTarget ||
                       screen_ == Screen::Pokedex;
  if (!artRows) return;
  const int start = pageStart();
  for (int local = 0; local < rowCount_; ++local) {
    const int y = listBounds_.y + local * rowHeight_ + 2;
    renderer.fillRect(listBounds_.x, y, 90, 60, false);
    uint16_t speciesId = 0;
    if (screen_ == Screen::Starter) speciesId = STARTERS[start + local];
    else if ((screen_ == Screen::Party || screen_ == Screen::Move || screen_ == Screen::ItemTarget) &&
             start + local < snapshot_.partyCount) speciesId = snapshot_.party[start + local].speciesId;
    else if (screen_ == Screen::Pc && local < static_cast<int>(pcCount_)) speciesId = pcPage_[local].speciesId;
    else if (screen_ == Screen::Pokedex &&
             pokemon::isSpeciesMarked(snapshot_.state.seenSpecies, static_cast<uint16_t>(start + local + 1))) {
      speciesId = static_cast<uint16_t>(start + local + 1);
    }
    if (screen_ == Screen::Bag) {
      pokemon::drawPokemonItemArt(renderer, static_cast<pokemon::EvolutionItem>(start + local + 1), false,
                                  Rect{listBounds_.x + 5, y, 80, 60}, false);
    } else if (speciesId != 0) {
      // The 40x30 menu files are intentionally native-sized and GfxRenderer
      // does not upscale. Use the same approved icon's 120x90 presentation
      // copy so it can be reduced cleanly into the row instead of appearing
      // as a tiny 40x30 mark on the X3 panel.
      pokemon::drawPokemonSpeciesArt(renderer, speciesId, true, Rect{listBounds_.x + 5, y, 80, 60});
    }
  }
}

void PokemonActivity::renderHeaderAndHints() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const char* title = tr(STR_POKEMON);
  if (screen_ == Screen::Party || screen_ == Screen::Move || screen_ == Screen::ItemTarget) title = tr(STR_POKEMON_PARTY);
  else if (screen_ == Screen::Pc || screen_ == Screen::PcOrder) title = tr(STR_POKEMON_PC_BOX);
  else if (screen_ == Screen::Bag) title = tr(STR_POKEMON_BAG);
  else if (screen_ == Screen::Pokedex) title = tr(STR_POKEDEX);
  else if (screen_ == Screen::Summary || screen_ == Screen::Actions) title = tr(STR_POKEMON_SUMMARY);
  const Rect header{0, metrics.topPadding, renderer.getScreenWidth(), TouchHeaderBackButton::height(metrics, mappedInput)};
  if (mappedInput.hasTouchHardware()) TouchHeaderBackButton::draw(renderer, uiTarget_, header, title, false);
  else GUI.drawHeader(renderer, header, title);
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void PokemonActivity::render(RenderLock&&) {
  renderer.clearScreen();
  renderHeaderAndHints();
  uiReady_ = false;
  app_.render();
  uiReady_ = true;
  renderFocused();
  renderRowArt();
  renderer.displayBuffer();
}

#endif
