#include <cstdlib>
#include <cstring>
#include <functional>
#include <future>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <curl/curl.h>
#include <curl/multi.h>
#include <ftxui/component/app.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/loop.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <libxml/HTMLparser.h>
#include <libxml/HTMLtree.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "global.h"
#include "xml.h"

using namespace std::chrono_literals;

SharedBuffer sb{};

int main(int argc, char *argv[]) {
  using namespace ftxui;

  CurlGlobal::init();
  DefeedCtx::init();
  DefeedCtx::setup_dirs();

  // Main Thread

  bool show_logs = false;
  bool fetching_rss = false;
  std::future<std::vector<std::unique_ptr<XML::NodeBase>>> f;

  size_t timeout = 0;
  int count = 0;
  auto screen = App::Fullscreen();
  Component loading = Renderer([&fetching_rss] {
    static int img;
    constexpr int idx = 15;
    if (!fetching_rss) {
      return emptyElement();
    }
    img = (img + 1) % 200;
    return spinner(idx, img);
  });

  int log_menu_idx = 0;
  Component log_menu = Menu(&sb.getEntries(), &log_menu_idx);
  Component log = Renderer(log_menu, [&] {
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

  Component status_line = Renderer([&] {
    std::string status = (fetching_rss ? "Fetching" : "Normal");

    // clang-format off
    return hbox({
      text(" "),
      text(status),
      text(" "),
      loading->Render(),
      filler() | yflex,
    }) | bgcolor(Color::Blue)
       | color(Color::White);
    // clang-format off
  });

  Component renderer = Renderer([&] {
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

  Component Home = Container::Vertical({});
  Home->Add(renderer);
  Home->Add(log);

  Home |= CatchEvent([&](Event event) {
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
    return false;
  });

  Loop l(&screen, Home);
  while (!l.HasQuitted()) {
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

  CurlGlobal::cleanup();
}
