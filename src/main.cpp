#include "curl.h"
#include "defeed.h"
#include "sb.h"
#include "ui.h"

// Global Shared Buffer
SharedBuffer sb{};

extern "C" void write_to_sb(int a) { sb.append(a); }

int main(int argc, char *argv[]) {
  CurlGlobal defeed_curl{};
  DefeedCtx::init();
  DefeedCtx::setupDirs();

  // Main Thread
  UI::App defeed{};
  defeed.event_loop();

  write_to_sb(12); // a call so that compiler doesn't optimize it out
}

// BUG: mcyoung and stephendeihl fetched xml is entity encoded?
