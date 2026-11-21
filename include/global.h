#pragma once

#include <algorithm>
#include <fstream>
#include <string>
#include <vector>

#include "types.h"
#include "xml.h"

extern SharedBuffer sb;

namespace Utils {

// impl from cppreference
inline std::string strToLower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return s;
}

inline std::string strStrip(std::string s) {
  s.erase(s.find_last_not_of(" \t") + 1);
  s.erase(0, s.find_first_not_of(" \t"));
  return s;
}

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

} // namespace Utils
