#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "xml.h"

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
  static std::vector<std::unique_ptr<XML::NodeBase>> fetchRSS();
};

class SharedBuffer {
public:
  SharedBuffer();
  std::string getString();
  const std::vector<std::string_view> &getEntries();
  const std::size_t getTotalEntries();
  void append(const std::string &s);

private:
  std::mutex mtx;
  std::vector<char> buf;
  std::vector<std::string_view> entries;

  void lock();
  void unlock();
};

extern SharedBuffer sb;
