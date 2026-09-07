#pragma once

#include <string>

struct CurlGlobal {
  static void init();
  static void cleanup();

private:
  CurlGlobal();
};

struct DefeedCtx {
  static inline std::string home{}, defeed{};
  static inline std::string rss{}, rss_txt{}, rss_info{};
  static void init();
  static void setup_dirs();
};
