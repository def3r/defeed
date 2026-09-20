#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
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

#include "fetcher.h"
#include "ftxui/dom/elements.hpp"
#include "global.h"
#include "xml.h"

using namespace std::chrono_literals;

std::vector<std::unique_ptr<XML::NodeBase>> fetchRSS() {
  std::this_thread::sleep_for(6s);
  return {};
  std::ifstream rss_txt{DefeedCtx::rss_txt};
  if (!rss_txt.is_open()) {
    std::cerr << "Cant open file " << DefeedCtx::rss_txt << std::endl;
    std::exit(1);
  }

  std::vector<std::pair<std::string, size_t>> rss_urls{};
  while (!rss_txt.eof()) {
    std::string url{};
    std::getline(rss_txt, url);
    if (!url.empty()) {
      size_t hash = std::hash<std::string>{}(url);
      std::cout << hash << "\t\t" << url << std::endl;
      rss_urls.push_back({url, hash});
    }
  }
  rss_txt.close();

  MultiFetcher fetch_rss{};
  MultiFetcher cond_fetch_rss{};
  for (auto [url, hash] : rss_urls) {
    std::filesystem::path url_path{DefeedCtx::rss + "/" + std::to_string(hash)};
    if (!std::filesystem::is_directory(url_path)) {
      fetch_rss.add(url);
      continue;
    }
    std::ifstream etag_file{url_path.string() + "/etag"};
    if (!etag_file.is_open()) {
      std::cout << "Can't find etag file for " << hash << " (" << url << ")"
                << std::endl;
      std::cout << "\t" << "Fetching feed for " << hash << " (" << url << ")"
                << std::endl;
      fetch_rss.add(url);
      continue;
    }
    std::string etag{};
    etag_file >> etag;
    etag_file.close();

    std::cout << "If-None-Match: " << etag << std::endl;

    Fetcher f{url, etag};
    f.append_headers("If-None-Match: " + etag);
    f.apply_headers();
    cond_fetch_rss.add(std::move(f));
  }

  fetch_rss.perform_write();
  cond_fetch_rss.perform_write();

  std::vector<std::unique_ptr<XML::NodeBase>> nodes;

  for (auto [url, hash] : rss_urls) {
    const std::string dir_name{DefeedCtx::rss + "/" + std::to_string(hash)};
    const std::string file_name{dir_name + "/rssfeed.txt"};

    XML::Extract x{file_name};
    x.extract();

    nodes.emplace_back(x.get_root());
  }

  return nodes;
}

int main(int argc, char *argv[]) {
  using namespace ftxui;

  CurlGlobal::init();
  DefeedCtx::init();
  DefeedCtx::setup_dirs();

  // Main Thread

  // Create a simple document with three text elements.
  Element document = hbox({
      text("left") | border,
      text("middle") | border,
      text("right") | border,
  });

  // Create a screen with full width and height fitting the document.
  // auto screen = Screen::Create(Dimension::Full(), // Width
  //                              Dimension::Full()  // Height
  // );

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
              loading->Render()});

    return home;
  });

  Component Home = Container::Vertical({});
  Home->Add(renderer);
  Home->Add(loading);
  Home->Add(Renderer([] { return text("Bad Docs"); }));

  Home |= CatchEvent([&](Event event) {
    if (event == Event::Character('q')) {
      screen.ExitLoopClosure()();
      return true;
    }
    if (event == Event::Character('r')) {
      if (!fetching_rss) {
        f = std::async(std::launch::async, &fetchRSS);
        fetching_rss = true;
      }
      return true;
    }
    return false;
  });

  // screen.Loop(component);

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
