#include "ui.h"
#include "global.h"

#include <cassert>
#include <future>
#include <iostream>
#include <thread>

#include <ftxui/component/app.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/loop.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

using namespace std::chrono_literals;

extern SharedBuffer sb;

namespace UI {

using namespace ftxui;

void App::init_comp_loading() {
  loading = Renderer([this] {
    static int img;
    constexpr int idx = 15;
    if (!fetching_rss) {
      return emptyElement();
    }
    img = (img + 1) % 200;
    return spinner(idx, img);
  });
}

void App::init_comp_log() {
  log_menu = Menu(&sb.getEntries(), &log_menu_idx);
  log = Renderer(log_menu, [this] {
    static std::size_t last_entry_idx = log_menu_idx;

    if (!show_logs) {
      return emptyElement();
    }

    std::size_t cur_entry_count = sb.getTotalEntries();
    if (last_entry_idx != cur_entry_count - 1) {
      if (log_menu_idx == last_entry_idx) {
        log_menu_idx = cur_entry_count - 1;
      }
    }
    last_entry_idx = cur_entry_count - 1;

    return log_menu->Render() | vscroll_indicator | yframe |
           size(ftxui::HEIGHT, ftxui::EQUAL, 10);
  });
}

void App::init_comp_status_line() {
  status_line = Renderer([this] {
    static std::string status;
    if (mode == Mode::Normal)
      status = (fetching_rss ? "Fetching" : "Normal");
    else
      status = "Cmd";

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
  });
}

void App::init_comp_cmdline() {
  InputOption cmd_line_style = InputOption::Default();
  cmd_line_style.transform = [](InputState state) {
    state.element |= color(Color::White);
    return state.element;
  };
  input = Input(&cmd, cmd_line_style);
  input |= CatchEvent([this](Event event) {
    if (event == Event::Escape) {
      mode = Mode::Normal;
      return true;
    }

    if (event == Event::Return) {
      mode = Mode::Normal;
      if (cmd_exec_unit.Exec(cmd)) {
        cmd = "";
      }
      return true;
    }

    return false;
  });
  cmd_line = Renderer(input, [this] {
    if (mode != Mode::Cmd) {
      return emptyElement();
    }
    return input->Render();
  });
}

void App::init_comp_renderer() {
  renderer = Renderer([this] {
    static int ren_count;
    if (count != ren_count) {
      ren_count = count;
    }

    // clang-format off
    Element home = vbox({
      text("Home screen"),
      text("Press r to reload rss"),
      text("Press q to quit"),
      text("Press l to toggle logs"),
      separator(),
      text("Pressed r " + std::to_string(count) + " times"),
      text("Timeout Count  " + std::to_string(timeout) + " times"),

      filler(),

      status_line->Render()
    }) | flex;
    // clang-format on

    return home;
  });
}

void App::init_comp_home() {
  Home = Container::Vertical({});
  Home->Add(renderer);
  Home->Add(log);
  Home->Add(cmd_line);

  Home |= CatchEvent([this](Event event) {
    if (mode != Mode::Normal)
      return false;

    if (event == Event::Character('q')) {
      screen.ExitLoopClosure()();
      return true;
    }

    if (event == Event::Character('r')) {
      if (!fetching_rss) {
        f = std::async(std::launch::async, &DefeedCtx::fetchRSS);
        fetching_rss = true;
      }
      return true;
    }

    if (event == Event::Character('l')) {
      show_logs = !show_logs;
      return true;
    }

    if (event == Event::Character(':')) {
      mode = Mode::Cmd;
      input->TakeFocus(); // Forcefully give it to the input else we need to
                          // press down arrow key or `j`
      return true;
    }

    return event.is_character();
  });
}

void App::initialize_components() {
  init_comp_loading();
  init_comp_log();
  init_comp_status_line();
  init_comp_cmdline();
  init_comp_renderer();
  init_comp_home();
}

App::App() : screen(ftxui::App::Fullscreen()), cmd_exec_unit(this) {
  initialize_components();
}

void App::EventLoop() {
  Loop l(&screen, Home);
  while (!l.HasQuitted() && !quit) {
    if (fetching_rss) {
      switch (std::future_status status = f.wait_for(100ms); status) {
      case std::future_status::deferred:
        std::cerr << "main_thread: Illegal defer; Must not defer rss fetch"
                  << std::endl;
        std::abort();

      case std::future_status::ready:
        fetching_rss = false;
        count++;
        break;

      case std::future_status::timeout:
        timeout++;
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
    parent->show_logs = !parent->show_logs;
  } else if (cmd == "q") {
    parent->quit = true;
  } else {
    retval = false;
  }

  return retval;
}

CmdLine::CmdLine(UI::App *parent) : parent(parent) {
  assert(parent && "CmdLine cannot have nullptr for parent");
}

} // namespace UI
