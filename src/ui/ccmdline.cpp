#include "defeed.h"
#include "global.h"
#include "ui.h"

namespace UI {

using namespace ftxui;

CmdLineComponent::CmdLineComponent(CmdLine *_Nonnull clunit) : clunit(clunit) {
  InputOption cmd_line_style = InputOption::Default();
  cmd_line_style.transform = [&](InputState state) {
    state.element |= color(textcolor);
    return state.element;
  };
  Component input = Input(&cmd, cmd_line_style);
  Add(input);
}

bool CmdLineComponent::Focusable() const { return true; }

Element CmdLineComponent::OnRender() {
  if (DefeedCtx::isModeSearch()) {
    if (clunit->is_search_success()) {
      textcolor = Color::White;
    } else {
      textcolor = Color::OrangeRed1;
    }
  }
  return Render();
}

bool CmdLineComponent::OnEvent(Event ev) {
  if (not DefeedCtx::isModeCmd() and not DefeedCtx::isModeSearch())
    return false;

  EVENT_HANDLER_PROLOGUE;

  REGISTER_EV("Leave to Normal mode", Escape) {
    if (DefeedCtx::isModeSearch()) {
      this->clunit->exit_search(ev);
      textcolor = Color::White;
    }
    DefeedCtx::mode = DefeedCtx::Mode::Normal;
    DefeedCtx::shift_focus = true;
  }

  REGISTER_EV("Execute and leave to Normal mode", Return) {
    if (DefeedCtx::isModeCmd()) {
      this->clunit->exec(cmd);
    } else if (DefeedCtx::isModeSearch()) {
      this->clunit->exit_search(ev);
      DefeedCtx::shift_focus = true;
      cmd = "";
      textcolor = Color::White;
    }
    DefeedCtx::mode = DefeedCtx::Mode::Normal;
  }

  // TODO: empty string details for register means private registeration
  REGISTER("", DefeedCtx::isModeSearch()) {
    ComponentBase::OnEvent(ev);
    this->clunit->search(cmd);
  }

  EVENT_HANDLER_EPILOGUE;

  return ComponentBase::OnEvent(ev);
}

} // namespace UI
