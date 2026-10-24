#include "ui.h"
#include "global.h"

#include <cassert>
#include <future>
#include <iostream>
#include <memory>
#include <thread>

#include <ftxui/component/app.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/loop.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

using namespace std::chrono_literals;

namespace UI {

using namespace ftxui;

Element LoadingComponent::OnRender() {
  static int img;
  constexpr int idx = 15;
  img = (img + 1) % 200;
  return spinner(idx, img);
}

LogMenuComponent::LogMenuComponent() {
  this->log_menu = Menu(&sb.getEntries(), &log_menu_idx);
  Add(log_menu);
}
bool LogMenuComponent::Focusable() const { return true; }
Element LogMenuComponent::OnRender() {
  static std::size_t last_entry_idx = log_menu_idx;

  std::size_t cur_entry_count = sb.getTotalEntries();
  if (last_entry_idx != cur_entry_count - 1) {
    if (log_menu_idx == last_entry_idx) {
      log_menu_idx = cur_entry_count - 1;
    }
  }
  last_entry_idx = cur_entry_count - 1;

  return log_menu->Render() | vscroll_indicator | yframe |
         size(ftxui::HEIGHT, ftxui::EQUAL, 10);
}

StatusLineComponent::StatusLineComponent() {
  loading = std::make_unique<LoadingComponent>();
  loading |= Maybe([]() { return DefeedCtx::task != DefeedCtx::Task::Ready; });
  Add(loading);
}
Element StatusLineComponent::OnRender() {
  static std::string status;
  // clang-format off
  if (DefeedCtx::mode == DefeedCtx::Mode::Normal) {
    switch (DefeedCtx::task) {
      case DefeedCtx::Task::Ready: status = "Normal"; break;
      case DefeedCtx::Task::Fetch: status = "Fetching"; break;
      case DefeedCtx::Task::Load:  status = "Loading"; break;
    }
  } else status = "Cmd";
  // clang-format on

  // clang-format off
  return hbox({
    text(" "),
    text(status),
    text(" "),
    loading->Render(),
    filler() | yflex,
  }) | bgcolor(Color::Blue)
     | color(Color::White);
  // clang-format on
}

CmdLineComponent::CmdLineComponent(CmdLine *_Nonnull clunit) : clunit(clunit) {
  InputOption cmd_line_style = InputOption::Default();
  cmd_line_style.transform = [](InputState state) {
    state.element |= color(Color::White);
    return state.element;
  };

  input = Input(&cmd, cmd_line_style);
  input |= CatchEvent([this](Event event) {
    if (DefeedCtx::mode != DefeedCtx::Mode::Cmd)
      return false;

    if (event == Event::Escape) {
      DefeedCtx::mode = DefeedCtx::Mode::Normal;
      return true;
    }

    if (event == Event::Return) {
      DefeedCtx::mode = DefeedCtx::Mode::Normal;
      if (this->clunit->Exec(cmd)) {
        cmd = "";
      }
      return true;
    }

    return false;
  });
  Add(input);
}
bool CmdLineComponent::Focusable() const { return true; }
Element CmdLineComponent::OnRender() { return input->Render(); }

bool Page::Focusable() const { return true; }

MenuOption Page::menuStyle() {
  static MenuOption option = MenuOption::Vertical();
  static short init = 0;
  if (init)
    return option;

  option.entries_option.transform = [](EntryState state) {
    Element e = state.active ? text("[ " + state.label + " ]")
                             : text("  " + state.label + "  ");
    if (state.focused)
      e = e | bold;
    if (state.focused)
      e = e | color(Color::Blue);

    if (state.active)
      e = e | bold;
    return e;
  };
  init++;
  return option;
}

LandingPage::LandingPage(const std::vector<std::string> &entries)
    : entries(entries) {
  home_menu = Menu(entries, &selected, Page::menuStyle());
  home_menu |= CatchEvent([this](Event ev) {
    if (ev == Event::Return) {
    }
    return false;
  });
  Add(home_menu);
}
Element LandingPage::OnRender() {
  // clang-format off
  Element home = vbox({
    text("Home screen"),
    separator(),
    home_menu->Render(),
  }) | flex;
  // clang-format on

  return home;
}

void App::init_app() {
  // clang-format off
  app = Container::Vertical({
    page,
    status_line,
    log,
    cmd_line
  });
  // clang-format on

  app |= CatchEvent([this](Event event) {
    if (DefeedCtx::mode != DefeedCtx::Mode::Normal)
      return false;

    if (event == Event::Character('q')) {
      if (DefeedCtx::show_logs)
        DefeedCtx::show_logs = false;
      else
        screen.ExitLoopClosure()();
      return true;
    }

    if (event == Event::Character('r')) {
      begin_fetch();
      return true;
    }

    if (event == Event::Character('l')) {
      DefeedCtx::show_logs = !DefeedCtx::show_logs;
      log->TakeFocus();
      return true;
    }

    if (event == Event::Character(':')) {
      DefeedCtx::mode = DefeedCtx::Mode::Cmd;
      cmd_line->TakeFocus(); // Forcefully give it to the input else we need to
                             // press down arrow key or `j`
      return true;
    }

    return false;
  });
}

// clang-format off
void App::initialize_components() {
  log         = std::make_unique<LogMenuComponent>();
  page        = std::make_unique<LandingPage>(make_entries_from_feed());
  cmd_line    = std::make_unique<CmdLineComponent>(&cmd_exec_unit);
  status_line = std::make_unique<StatusLineComponent>();

  log      |= Maybe(&DefeedCtx::show_logs);
  cmd_line |= Maybe([this] { return DefeedCtx::mode == DefeedCtx::Mode::Cmd; });

  init_app();
}
// clang-format on

void App::begin_fetch() {
  if (DefeedCtx::task != DefeedCtx::Task::Fetch) {
    f = std::async(std::launch::async, &DefeedCtx::fetchRSS);
    DefeedCtx::task = DefeedCtx::Task::Fetch;
  }
}

std::vector<std::string> App::make_entries_from_feed() {
  std::vector<std::string> entries{};
  entries.reserve(feeds.size());
  for (int i = 0; i < feeds.size(); i++) {
    entries.push_back(feeds[i].get()->url);
  }
  return entries;
}

App::App() : screen(ftxui::App::Fullscreen()), cmd_exec_unit(this) {
  // TODO: set DefeedCtx::task to Task::Load when we move to async
  feeds = DefeedCtx::loadRSS();
  initialize_components();
}

void App::EventLoop() {
  Loop l(&screen, app);
  while (!l.HasQuitted() && !DefeedCtx::quit) {
    if (DefeedCtx::task == DefeedCtx::Task::Fetch) {
      switch (std::future_status status = f.wait_for(100ms); status) {
      case std::future_status::deferred:
        std::cerr << "main_thread: Illegal defer; Must not defer rss fetch"
                  << std::endl;
        std::abort();

      case std::future_status::ready:
        DefeedCtx::task = DefeedCtx::Task::Ready;
        break;

      case std::future_status::timeout:
        break;
      };
    }

    screen.RequestAnimationFrame();

    l.RunOnce();

    // 60fps
    std::this_thread::sleep_for(std::chrono::milliseconds(1000 / 60));
  }
}

bool CmdLine::Exec(std::string &cmd) {
  // clean the stinky string
  cmd.erase(cmd.find_last_not_of(" \t") + 1);
  cmd.erase(0, cmd.find_first_not_of(" \t"));

  bool retval = true;

  if (cmd == "log") {
    DefeedCtx::show_logs = !DefeedCtx::show_logs;
    parent->log->TakeFocus();
  } else if (cmd == "q") {
    DefeedCtx::quit = true;
  } else if (cmd == "fetch") {
    parent->begin_fetch();
  } else {
    retval = false;
  }

  return retval;
}

CmdLine::CmdLine(UI::App *parent) : parent(parent) {
  assert(parent && "CmdLine cannot have nullptr for parent");
}

} // namespace UI
