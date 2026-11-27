#pragma once

#include <fstream>
#include <string>
#include <vector>
#include <stack>

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
  Walker(XML::NodeBase *_Nonnull feed);
  void walk_internal(int level, XML::InternalBase *root);
  void walk_leaf(int level, XML::LeafBase *root);
};

struct ConfigWriter {
  inline ConfigWriter(const std::string& fout) : fout(fout) { export_config(); }
  void export_config();

private:
  const std::string fout;
  void export_internal(std::ofstream& fs, XML::Internal root,
      std::stack<std::string>& fstack, const std::string& leaf_separator);
};

}; // namespace DefeedCtx
