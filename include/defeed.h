#pragma once

#include <fstream>
#include <string>
#include <vector>

#include "page.h"
#include "xml.h"

// Application State/Context
//
// (UI also uses this global state insteading of "owning" it)
namespace DefeedCtx {

// clang-format off
enum class Mode       { Normal,    Cmd,     Search        };
enum class Task       { Ready,     Load,    Fetch         };
enum class SwitchPage { Backwards, Forward, UndoBackwards };

using UrlPair  = std::pair<std::string, size_t>;
using UrlPairs = std::vector<UrlPair>;

inline std::string home{}, defeed{};
inline std::string rss{}, rss_xml{}, rss_info{};
inline std::string defeed_xml{};

// mostly UI specific
inline bool       quit = false;
inline bool       show_logs = false;
inline Task       task = Task::Ready;
inline Mode       mode = Mode::Normal;
inline bool       shift_focus = false;
inline bool       switch_request = false; // Page switch
inline SwitchPage switch_type = SwitchPage::Forward;
inline Schemas    toplevel{};
inline bool       refetch_rss = false;
inline std::unordered_map<size_t, char>
                  refetch_rss_map{};

void     init();
void     setupDirs();
UrlPairs getURLs();

void      fetchRSS();
XML::Node loadRSS(const std::string &url);
Strings   readToplevel();

inline bool isModeNormal() { return mode == Mode::Normal; }
inline bool isModeCmd()    { return mode == Mode::Cmd; };
inline bool isModeSearch() { return mode == Mode::Search; };

// clang-format on

struct Walker {
  static inline const std::string filename = "walker.txt";
  std::fstream fs{};
  XML::NodeBase *root = nullptr;
  inline Walker(XML::NodeBase *_Nonnull feed) : root(feed) {
    fs.open(filename, std::ios::out);
    fs << "[ROOT]\n";
    walk_internal(0, (XML::InternalBase *)root);
    fs.close();
  }

  inline void walk_internal(int level, XML::InternalBase *root) {
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

  inline void walk_leaf(int level, XML::LeafBase *root) {
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

}; // namespace DefeedCtx
