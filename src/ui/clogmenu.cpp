#include "defeed.h"
#include "global.h"
#include "sb.h"
#include "ui.h"
#include "uinav.h"

namespace UI {

using namespace ftxui;

LogMenuComponent::LogMenuComponent() {
  log_menu_idx = 0;
  entries = sb.get_entries();
  Component log_menu =
      Menu(&entries, &log_menu_idx, LogMenuComponent::menu_style());
  Add(log_menu);
}

bool LogMenuComponent::Focusable() const { return DefeedCtx::show_logs; }

Element LogMenuComponent::OnRender() {
  entries = sb.get_entries();
  static std::size_t last_entry_idx = log_menu_idx;

  std::size_t cur_entry_count = entries.size();
  if (last_entry_idx != cur_entry_count - 1) {
    if (log_menu_idx == last_entry_idx) {
      log_menu_idx = cur_entry_count - 1;
    }
  }
  last_entry_idx = cur_entry_count - 1;

  return Render() | vscroll_indicator | yframe;
}

bool LogMenuComponent::OnEvent(Event ev) {
  EVENT_HANDLER_PROLOGUE;

  REGISTER("", Navigation::vim(ev, log_menu_idx, entries.size())) {}

  REGISTER_CHAR("quit", q) {
    DefeedCtx::show_logs = false;
    DefeedCtx::shift_focus = true;
  }

  REGISTER_EV("shift focus back to main list page", CtrlK) {
    DefeedCtx::shift_focus = true;
  }

  EVENT_HANDLER_EPILOGUE;

  return ComponentBase::OnEvent(ev);
}

MenuOption LogMenuComponent::menu_style() {
  MenuOption option = MenuOption::Vertical();
  option.entries_option.transform = [](EntryState state) {
    Element e = text(" " + state.label + " ");
    if (state.active)
      e = e | bold | color(Color::LightCoral);
    return e;
  };
  return option;
}

} // namespace UI
