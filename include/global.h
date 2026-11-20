#pragma once

#include <algorithm>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include <ftxui/component/component_base.hpp>

#include "xml.h"

#define UNUSED

struct CurlGlobal {
  static void init();
  static void cleanup();

private:
  CurlGlobal();
};

using PageID = ssize_t; // Corresponds to index in PCB

// clang-format off
#define XM_Schema_Fields           \
  X(TITLE, "title",      1L << 1)  \
  X(LINK,  "link",       1L << 2)  \
  X(RSS,   "rss",        1L << 3)  \
  X(YT,    "yt",         1L << 4)  \
  X(NLTR,  "newsletter", 1L << 5)  \
  X(CTGRY, "category",   1L << 6)  \
  X(FNAME, "file",       1L << 7)  \
  X(DSPLY, "display",    1L << 8)  \
  /* Others */                     \
  X(OTHER, "",           1L << 31)
// clang-format on

// TODO: Evaluate if Schema can be merged directly with XML; they both do
// effectively the same thing
struct Schema;
using Schemas = std::vector<Schema>;
struct Schema {
  using SchemaField = size_t;

  PageID pid = -1;            // Is a page assoc with the cur schema, if not -1
  UNUSED bool linked = false; // If a link assoc with the cur schema is open
  SchemaField fields = 0;
  std::unordered_map<std::string, std::string> m;

  enum Field {
#define X(FIELD, _, FIELDVAL) FIELD = FIELDVAL,
    XM_Schema_Fields
#undef X
  };

  using fieldDecodeTy = std::map<SchemaField, std::string>;
  static inline std::map<SchemaField, std::string> fieldDecode = {
#define X(FIELD, FIELDNAME, _) {Field::FIELD, FIELDNAME},
      XM_Schema_Fields
#undef X
  };

  using fieldEncodeTy = std::map<std::string, SchemaField>;
  static inline std::map<std::string, SchemaField> fieldEncode = {
#define X(FIELD, FIELDNAME, _) {FIELDNAME, Field::FIELD},
      XM_Schema_Fields
#undef X
  };

  void put(const std::string &key, const std::string &val);
  bool find(const std::string &key) const;
  static std::vector<std::string> getAll(const Schemas &ss,
                                         const std::string &key);
  bool hasPage();
};

// NOTE: toplevel page always has id = 0
struct PageCtx {
  PageID id = -1, pid = -1;

  // A page can either hold ss, or root;
  // Order or evaluation:
  //   if ss.size() == 0 -> root
  Schemas ss{};
  XML::Node root{};

  int selected = 0;
  std::string entry_key = "";
  std::vector<std::string> entries{};
};

namespace PCB {

inline std::vector<PageCtx> pctx{};

// create a page using schemas
PageID create(const Schemas &ss);
PageID create(const Schemas &ss, const std::string &entry_key);

// create a page using schema file name
PageID create(const std::string fname);

// create a page using XML::Node
PageID create(XML::Node root, const std::string &entry_key,
              const std::vector<std::string> &entries);

PageCtx &getRefTo(const PageID pid);
Schema &getSchemaAt(const PageID pid, const size_t index);

// For temporary pages
//
// temporary pages have 31st bit set in the PageID
namespace Temp {

UNUSED inline std::unordered_map<PageID, PageCtx> t_pctx{};
inline PageCtx tp{};

// create a temporary page using schema file name
PageID create(const Schemas &ss);

}; // namespace Temp

}; // namespace PCB

// FIXME: Get rid of ts
struct RssFeed {
  std::string url{};
  std::size_t hash{};

  std::string author{};
  std::string last_updated{};

  std::shared_ptr<XML::NodeBase> root{};

  int list_page_selected = 0;
  int list_page_max_entries = 0;
  ftxui::Component list_page_menu = nullptr;
};

// Application State/Context
//
// (UI also uses this global state insteading of "owning" it)
namespace DefeedCtx {

enum class Mode { Normal, Cmd, Search };
enum class Task { Ready, Load, Fetch };
enum class SwitchPage { Backwards, Forward, UndoBackwards };

// clang-format off
using UrlPair  = std::pair<std::string, size_t>;
using UrlPairs = std::vector<UrlPair>;
// clang-format on

inline std::string home{}, defeed{};
inline std::string rss{}, rss_xml{}, rss_info{};
inline std::string defeed_xml{};

// mostly UI specific
inline bool quit = false;
inline bool show_logs = false;
inline Task task = Task::Ready;
inline Mode mode = Mode::Normal;
inline bool shift_focus = false;
inline bool switch_request = false; // Page switch
inline SwitchPage switch_type = SwitchPage::Forward;
inline Schemas toplevel{};

void init();
void setup_dirs();
UrlPairs get_urls();

std::vector<std::shared_ptr<RssFeed>> fetchRSS();
std::vector<std::shared_ptr<RssFeed>> loadRSS();
XML::Node loadRSS(const std::string &url);
std::vector<std::string> read_toplevel();

}; // namespace DefeedCtx

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
  std::mutex mtx{};
  std::vector<char> buf{};
  std::vector<std::string_view> entries{};
  std::fstream lfile{};

  void lock();
  void unlock();
};

struct Walker {
  std::string filename = "walker.txt";
  std::fstream fs{};
  XML::NodeBase *root = nullptr;
  Walker(RssFeed *_Nonnull feed) : root(feed->root.get()) {
    fs.open(filename, std::ios::out);
    fs << "[ROOT]\n";
    walk_internal(0, (XML::InternalBase *)root);
    fs.close();
  }
  Walker(XML::NodeBase *_Nonnull feed) : root(feed) {
    fs.open(filename, std::ios::out);
    fs << "[ROOT]\n";
    walk_internal(0, (XML::InternalBase *)root);
    fs.close();
  }

  void walk_internal(int level, XML::InternalBase *root) {
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
          walk_internal(level + 4, (XML::InternalBase *)child);
        } else {
          walk_leaf(level + 4, (XML::LeafBase *)child);
        }
      }
    }
  }

  void walk_leaf(int level, XML::LeafBase *root) {
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

namespace Utils {

// impl from cppreference
inline std::string str_tolower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return s;
}

} // namespace Utils

extern SharedBuffer sb;
