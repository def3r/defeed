#include "defeed.h"
#include "ui.h"
#include "uinav.h"

namespace UI {

using namespace ftxui;

bool Page::Focusable() const { return true; }

MenuOption Page::menu_style() {
  MenuOption option = MenuOption::Vertical();

  option.entries_option.transform = [](EntryState state) {
    Element e = state.active ? text("[ " + state.label + " ]")
                             : text("  " + state.label + "  ");
    if (state.active)
      e = e | bold | color(Color::Blue);
    return e;
  };
  return option;
}

int Page::GetCurIndex() const { return selected; }

int Page::GetMaxIndex() const { return max_entries; }

bool Page::OnEvent(Event ev) {
  if (Navigation::vim(ev, selected, max_entries)) {
    return true;
  }

  // TODO: Switch Request
  if (ev == Event::Escape || ev == Event::Character('-')) {
    DefeedCtx::switch_request = true;
    DefeedCtx::switch_type = DefeedCtx::SwitchPage::Backwards;
    return true;
  }

  if (ev == Event::Return) {
    DefeedCtx::switch_request = true;
    DefeedCtx::switch_type = DefeedCtx::SwitchPage::Forward;
    return true;
  }

  if (ev == Event::Character('+')) {
    DefeedCtx::switch_request = true;
    DefeedCtx::switch_type = DefeedCtx::SwitchPage::UndoBackwards;
    return true;
  }

  return ComponentBase::OnEvent(ev);
}

PageID Page::get_pid() { return this->pid; }

} // namespace UI
