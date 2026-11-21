#include "defeed.h"
#include "ui.h"

namespace UI {

using namespace ftxui;

CmdLineComponent::CmdLineComponent(CmdLine *_Nonnull clunit) : clunit(clunit) {
  InputOption cmd_line_style = InputOption::Default();
  cmd_line_style.transform = [](InputState state) {
    state.element |= color(Color::White);
    return state.element;
  };
  Component input = Input(&cmd, cmd_line_style);
  Add(input);
}

bool CmdLineComponent::Focusable() const { return true; }

Element CmdLineComponent::OnRender() { return Render(); }

bool CmdLineComponent::OnEvent(Event ev) {
  if (not DefeedCtx::isModeCmd() && not DefeedCtx::isModeSearch())
    return false;

  if (ev == Event::Escape) {
    if (DefeedCtx::isModeSearch()) {
      this->clunit->exit_search(ev);
    }
    DefeedCtx::mode = DefeedCtx::Mode::Normal;
    DefeedCtx::shift_focus = true;
    return true;
  }

  if (ev == Event::Return) {
    if (DefeedCtx::isModeCmd()) {
      this->clunit->exec(cmd);
    } else if (DefeedCtx::isModeSearch()) {
      this->clunit->exit_search(ev);
      DefeedCtx::shift_focus = true;
      cmd = "";
    }
    DefeedCtx::mode = DefeedCtx::Mode::Normal;
    return true;
  }

  if (DefeedCtx::isModeSearch()) {
    ComponentBase::OnEvent(ev);
    this->clunit->search(cmd);
    return true;
  }

  return ComponentBase::OnEvent(ev);
}

} // namespace UI
