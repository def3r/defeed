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

using Node = std::shared_ptr<NodeBase>;
using Children = std::vector<Node>;

struct InternalBase : public NodeBase {
  Children children;
  InternalBase();
  // TODO: Here Do we need to destroy the children explicitly?
};
using Internal = std::shared_ptr<InternalBase>;

struct LeafBase : public NodeBase {
  std::string content;
  LeafBase();
};
using Leaf = std::shared_ptr<LeafBase>;

// The only leaf node?
using TEXT = LeafBase;

static Node make_leaf(xmlNode *cur_node);
static Node make_internal(xmlNode *cur_node);
Node extract(const xmlChar *str);
void walk(HTML::NodeBase *root);

} // namespace HTML
