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

  Component log = Renderer([&] {
    const std::string s = sb.getString();
    return s.size() ? paragraph(s) : emptyElement();
  });

  Component renderer = Renderer([&] {
    static int ren_count;
    if (count != ren_count) {
      ren_count = count;
    }
    Element home =
        vbox({text("Home screen"), text("Press r to reload rss"),
              text("Press q to quit"), separator(),
              text("Pressed r " + std::to_string(count) + " times"),
              text("Timeout Count  " + std::to_string(timeout) + " times"),
              loading->Render(), log->Render()});

    return home;
  });

  Component Home = Container::Vertical({});
  Home->Add(renderer);
  Home->Add(Renderer([] { return text("Bad Docs"); }));

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
