#include "defeed.h"
#include "global.h"
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
  EVENT_HANDLER_PROLOGUE;

  REGISTER("", Navigation::vim(ev, selected, max_entries)) {}

  // TODO: Switch Request
  REGISTER("Move back in history",
           EVENT_IS(Escape) or EVENT_IS(Character('-'))) {
    DefeedCtx::switch_request = true;
    DefeedCtx::switch_type = DefeedCtx::SwitchPage::Backwards;
  }

  REGISTER_EV("Select current entry", Return) {
    DefeedCtx::switch_request = true;
    DefeedCtx::switch_type = DefeedCtx::SwitchPage::Forward;
  }

  REGISTER_CHAR("Move forward in history", +) {
    DefeedCtx::switch_request = true;
    DefeedCtx::switch_type = DefeedCtx::SwitchPage::UndoBackwards;
  }

  EVENT_HANDLER_EPILOGUE;

  return ComponentBase::OnEvent(ev);
}

PageID Page::get_pid() { return this->pid; }

} // namespace UI
