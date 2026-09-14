#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include "html.h"

namespace XML {

enum class NodeType { Internal, Leaf };

using Attributes = std::unordered_map<std::string, std::string>;

struct NodeBase {
  NodeType type;
  Attributes attrs;
};

using Children =
    std::unordered_map<std::string, std::vector<std::unique_ptr<NodeBase>>>;

struct Node : public NodeBase {
  Children children;

  Node();

  // TODO: Here Do we need to destroy the children explicitly?
};

struct Leaf : public NodeBase {
  std::variant<std::string, std::unique_ptr<HTML::NodeBase>> content;
  int content_idx;

  Leaf();
};

// clang-format off
using Channel       = Node;
using Item          = Node;
using Image         = Node;
using Title         = Leaf;
using Link          = Leaf;
using Description   = Leaf;
using Content       = Leaf;
using LastBuildDate = Leaf;
using PubDate       = Leaf;
using URL           = Leaf;
using Width         = Leaf;
using Height        = Leaf;
using GUID          = Leaf;
// clang-format on

class Extract {
public:
  Extract() = delete;
  ~Extract();

  Extract(const std::string &xml_file);
  // move constructor
  Extract(Extract &&other);
  // move assignment
  Extract &operator=(Extract &&other);

  Extract(const Extract &) = delete;
  Extract &operator=(const Extract &) = delete;

  // Calling function assign
  // Called  function allocates
  void extract();
  std::unique_ptr<NodeBase> get_root();

  void walk();

private:
  xmlDoc *doc;
  xmlNode *root;
  std::unique_ptr<NodeBase> root_node;

  std::unique_ptr<NodeBase> make_leaf(xmlNode *cur_node);
  std::unique_ptr<NodeBase> make_internal(xmlNode *cur_node);

  void walk_root_node(NodeBase *root);
};

} // namespace XML
