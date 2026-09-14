#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <libxml/HTMLparser.h>
#include <libxml/HTMLtree.h>

namespace HTML {

enum class NodeType { Internal, Leaf };
using Attributes = std::unordered_map<std::string, std::string>;

// Lets store NodeName as an internal attr
// attrs["HTMLNodeName"] contains tag name
struct NodeBase {
  NodeType type;
  Attributes attrs;
};

using Children = std::vector<std::unique_ptr<NodeBase>>;

struct Node : public NodeBase {
  Children children;
  Node();
  // TODO: Here Do we need to destroy the children explicitly?
};

struct Leaf : public NodeBase {
  std::string content;
  Leaf();
};

// The only leaf node?
using TEXT = Leaf;

static std::unique_ptr<HTML::NodeBase> make_leaf(xmlNode *cur_node);
static std::unique_ptr<HTML::NodeBase> make_internal(xmlNode *cur_node);
std::unique_ptr<HTML::NodeBase> extract(const xmlChar *str);
void walk(HTML::NodeBase *root);

} // namespace HTML
