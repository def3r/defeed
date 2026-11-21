#pragma once

#include <ftxui/component/event.hpp>
#include <ftxui/screen/terminal.hpp>

#include "defeed.h"

namespace UI {

using namespace ftxui;

namespace Navigation {

inline bool vim(const Event &ev, int &selected, const int max_entries) {
  static int g_count = 0;
  static int scroll_factor = -1;
  if (scroll_factor == -1) {
    Dimensions dims = ftxui::Terminal::Size();
    int vis = dims.dimy - (DefeedCtx::show_logs ? 12 : 2);
    scroll_factor = vis / 2; // half of visible page
  }

  if (ev == Event::Character("g")) {
    if ((++g_count % 2) == 0) {
      selected = 0;
    }
    return true;
  }
  g_count = 0;

  if (ev == Event::Character("G")) {
    selected = max_entries;
    return true;
  }

  if (ev == Event::CtrlD) {
    selected = std::min(selected + scroll_factor, max_entries);
    return true;
  } else if (ev == Event::CtrlU) {
    selected = std::max(selected - scroll_factor, 0);
    return true;
  }

  return false;
}

} // namespace Navigation

} // namespace UI
