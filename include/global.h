#pragma once

#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include <ftxui/component/component_base.hpp>

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

  std::shared_ptr<XML::NodeBase> root;

  int list_page_selected = 0;
  ftxui::Component list_page_menu = nullptr;
};

// Application State/Context
//
// (UI also uses this global state insteading of "owning" it)
struct DefeedCtx {
  enum class Mode { Normal, Cmd };
  enum class Task { Ready, Load, Fetch };
  enum class Page { Landing, List, Blog };

  // clang-format off
  using UrlPair  = std::pair<std::string, size_t>;
  using UrlPairs = std::vector<UrlPair>;
  // clang-format on

  static inline std::string home{}, defeed{};
  static inline std::string rss{}, rss_txt{}, rss_info{};

  // mostly UI specific
  static inline bool quit = false;
  static inline bool show_logs = false;
  static inline Task task = Task::Ready;
  static inline Mode mode = Mode::Normal;
  static inline Page page = Page::Landing;

  static void init();
  static void setup_dirs();
  static UrlPairs get_urls();

  static std::vector<std::shared_ptr<RssFeed>> fetchRSS();
  static std::vector<std::shared_ptr<RssFeed>> loadRSS();

private:
  DefeedCtx() = delete;
};

class SharedBuffer {
public:
  SharedBuffer();
  std::string getString();
  const std::vector<std::string_view> &getEntries();
  const std::size_t getTotalEntries();
  void append(const std::string &s);

  template <typename T> void append(T s) {
    std::stringstream ss;
    ss << s;
    append(ss.str());
  }

private:
  std::mutex mtx;
  std::vector<char> buf;
  std::vector<std::string_view> entries;
  std::fstream lfile;

  void lock();
  void unlock();
};

struct Walker {
  std::string filename = "walker.txt";
  std::fstream fs;
  XML::NodeBase *root;
  Walker(RssFeed *_Nonnull feed) : root(feed->root.get()) {
    fs.open(filename, std::ios::out);
    fs << "[ROOT]\n";
    walk_internal(0, (XML::Node *)root);
    fs.close();
  }

  void walk_internal(int level, XML::Node *root) {
    if (root == nullptr)
      return;

    std::string indent(level, ' ');
    std::string indent2(level + 2, ' ');
    for (auto it = root->children.begin(); it != root->children.end(); ++it) {
      fs << indent << it->first << std::endl;
      for (int i = 0; i < it->second.size(); i++) {
        fs << indent2 << it->first << "[" << i << "]" << std::endl;
        XML::NodeBase *child = it->second[i].get();
        if (child == nullptr) {
          fs << indent2 << "  NULLCHILD" << std::endl;
          continue;
        }
        if (child->type == XML::NodeType::Internal) {
          walk_internal(level + 4, (XML::Node *)child);
        } else {
          walk_leaf(level + 4, (XML::Leaf *)child);
        }
      }
    }
  }

  void walk_leaf(int level, XML::Leaf *root) {
    if (root == nullptr)
      return;

    std::string indent(level, ' ');
    if (root->content_idx == 0) {
      fs << indent << "[TEXT]: " << std::get<0>(root->content) << std::endl;
    } else {
      fs << indent << "[HTML]: " << std::endl;
    }
  }
};

extern SharedBuffer sb;
