#pragma once

#include <string>

class CurlGlobal {
private:
  CurlGlobal();

public:
  static void init();
  static void cleanup();
};

class DefeedCtx {
public:
  static inline std::string home{}, defeed{};
  static inline std::string rss{}, rss_txt{}, rss_info{};

  static void init();
};
