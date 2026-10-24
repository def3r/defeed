#pragma once

#include <fstream>
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

struct RssFeed {
  std::string url;
  std::size_t hash;

  std::string author;
  std::string last_updated;

  std::unique_ptr<XML::NodeBase> root;
};

struct DefeedCtx {
  // clang-format off
  using UrlPair  = std::pair<std::string, size_t>;
  using UrlPairs = std::vector<UrlPair>;
  // clang-format on

  static inline std::string home{}, defeed{};
  static inline std::string rss{}, rss_txt{}, rss_info{};
  static void init();
  static void setup_dirs();
  static UrlPairs get_urls();

  static std::vector<std::unique_ptr<RssFeed>> fetchRSS();
  static std::vector<std::unique_ptr<RssFeed>> loadRSS();
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
  std::fstream lfile;

  void lock();
  void unlock();
};

extern SharedBuffer sb;
