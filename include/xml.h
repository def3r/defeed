#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include "global.h"
#include "html.h"

namespace XML {

enum class NodeType { Internal, Leaf };

using Attributes = std::unordered_map<std::string, std::string>;

struct NodeBase {
  NodeType type = NodeType::Internal;
  Attributes attrs{};
};

// clang-format off
using Node     = std::shared_ptr<NodeBase>;
using Siblings = std::vector<Node>;
using Children = std::unordered_map<std::string, Siblings>;
// clang-format on

struct InternalBase : public NodeBase {
  Children children{};

  InternalBase();
  bool contains(const std::string &key);

  // TODO: Here Do we need to destroy the children explicitly?
};
using Internal = std::shared_ptr<InternalBase>;

struct LeafBase : public NodeBase {
  std::variant<std::string, HTML::Node> content{};
  int content_idx = 0;

  LeafBase();
  std::string get_content_string();
  bool is_content_string();
};
using Leaf = std::shared_ptr<XML::LeafBase>;

// clang-format off
using Channel       = InternalBase;
using Item          = InternalBase;
using Image         = InternalBase;
using Title         = LeafBase;
using Link          = LeafBase;
using Description   = LeafBase;
using Content       = LeafBase;
using LastBuildDate = LeafBase;
using PubDate       = LeafBase;
using URL           = LeafBase;
using Width         = LeafBase;
using Height        = LeafBase;
using GUID          = LeafBase;
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
  Node get_root();

  void walk();

private:
  std::string xml_file{};
  xmlDoc *doc = nullptr;
  xmlNode *root = nullptr;
  Node root_node{};

  Node make_leaf(xmlNode *cur_node);
  Node make_internal(xmlNode *cur_node);

  void walk_root_node(NodeBase *root);

  static bool shouldBeInternal(std::string &name);
};

//
// Cast from shared_ptr<NodeBase> to
//
inline XML::Internal cast_Internal(const XML::Node &FROM_NODE) {
  Utils::Assert(FROM_NODE->type == XML::NodeType::Internal,
                "Invalid cast to Internal XML Node");
  return std::static_pointer_cast<XML::InternalBase>(FROM_NODE);
}

inline XML::InternalBase *cast_InternalBase(const XML::Node &FROM_NODE) {
  Utils::Assert(FROM_NODE->type == XML::NodeType::Internal,
                "Invalid cast to Internal XML Node Base");
  return static_cast<XML::InternalBase *>(FROM_NODE.get());
}

inline XML::Leaf cast_Leaf(const XML::Node &FROM_NODE) {
  Utils::Assert(FROM_NODE->type == XML::NodeType::Leaf,
                "Invalid cast to Leaf XML Node");
  return std::static_pointer_cast<XML::LeafBase>(FROM_NODE);
}

inline XML::LeafBase *cast_LeafBase(const XML::Node &FROM_NODE) {
  Utils::Assert(FROM_NODE->type == XML::NodeType::Leaf,
                "Invalid cast to Leaf XML Node Base");
  return static_cast<XML::LeafBase *>(FROM_NODE.get());
}

inline void mustContain(Children &children, const std::string &key) {
  Utils::Assert(children.find(key) != children.end(),
                "children must contain " + key);
}

} // namespace XML
