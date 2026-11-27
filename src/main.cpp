#include "curl.h"
#include "defeed.h"
#include "sb.h"
#include "ui.h"

// Global Shared Buffer
SharedBuffer sb{};

int main(int argc, char *argv[]) {
  CurlGlobal defeed_curl{};
  DefeedCtx::init();
  DefeedCtx::setupDirs();

  // Main Thread
  UI::App defeed{};
  defeed.event_loop();
}

// BUG: mcyoung and stephendeihl fetched xml is entity encoded?
