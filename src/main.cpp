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
