#include "defeed.h"
#include "ui.h"

namespace UI {

using namespace ftxui;

StatusLineComponent::StatusLineComponent() {
  Component loading = std::make_unique<LoadingComponent>();
  loading |= Maybe([]() { return DefeedCtx::task != DefeedCtx::Task::Ready; });
  Add(loading);
}

Element StatusLineComponent::OnRender() {
  static std::string status;
  static Color c = Color::Blue;
  // clang-format off
  if (DefeedCtx::isModeNormal()) {
    switch (DefeedCtx::task) {
      case DefeedCtx::Task::Ready: status = "Normal"; break;
      case DefeedCtx::Task::Fetch: status = "Fetching"; break;
      case DefeedCtx::Task::Load:  status = "Loading"; break;
    }
  } else if (DefeedCtx::isModeCmd()) {
    status = "Cmd";
  } else if (DefeedCtx::isModeSearch()) {
    status = "Search";
    c = Color::Green;
  }

  if (not DefeedCtx::isModeSearch()) {
    c = Color::Blue;
  }

  return hbox({
    text(" "),
    text(status),
    text(" "),
    Render(),
    filler() | yflex,
  }) | bgcolor(c)
     | color(Color::White);
  // clang-format on
}

} // namespace UI
