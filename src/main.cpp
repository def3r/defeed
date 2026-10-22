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
#include "ui.h"

// Global Shared Buffer
SharedBuffer sb{};

int main(int argc, char *argv[]) {
  CurlGlobal::init();
  DefeedCtx::init();
  DefeedCtx::setup_dirs();

  // Main Thread
  UI::App defeed{};
  defeed.EventLoop();

  CurlGlobal::cleanup();
}

// BUG: mcyoung and stephendeihl fetched xml is entity encoded?
