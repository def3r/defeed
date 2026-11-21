#include "curl.h"
#include "defeed.h"
#include "sb.h"
#include "ui.h"

// Global Shared Buffer
SharedBuffer sb{};

int main(int argc, char *argv[]) {
  CurlGlobal::init();
  DefeedCtx::init();
  DefeedCtx::setupDirs();

  // Main Thread
  UI::App defeed{};
  defeed.event_loop();

  CurlGlobal::cleanup();
}

// BUG: mcyoung and stephendeihl fetched xml is entity encoded?
