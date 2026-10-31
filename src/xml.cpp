#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

#include "global.h"
#include "html.h"
#include "xml.h"

namespace XML {

InternalBase::InternalBase() {
  this->type = NodeType::Internal;
  this->attrs = {};
}

LeafBase::LeafBase() {
  this->type = NodeType::Leaf;
  this->attrs = {};
  content_idx = 0;
}

Extract::~Extract() {
  if (doc != nullptr) {
    xmlFreeDoc(doc);
  }
}

Extract::Extract(const std::string &xml_file) {
  this->xml_file = xml_file;
  doc = xmlReadFile(xml_file.c_str(), NULL, 0);
  if (doc == nullptr) {
    std::cerr << "XMLExtract: Unable to parse file " << xml_file << std::endl;
    std::abort();
  }

  root = xmlDocGetRootElement(doc);
  if (root == nullptr) {
    std::cerr << "XMLExtract: Unable to get root of the parsed doc for file "
              << xml_file << std::endl;
    std::abort();
  }

  root_node = nullptr;
}

// move constructor
Extract::Extract(Extract &&other) {
  this->doc = other.doc;
  this->root = other.root;
  this->root_node = std::move(other.root_node);

  other.doc = nullptr;
  other.root = nullptr;
  other.root_node = nullptr;
}

// move assignment
Extract &Extract::operator=(Extract &&other) {
  if (this != &other) {
    this->doc = other.doc;
    this->root = other.root;
    this->root_node = std::move(other.root_node);

    other.doc = nullptr;
    other.root = nullptr;
    other.root_node = nullptr;
  }

  return *this;
}

// Calling function assign
// Called  function allocates
void Extract::extract() {
  root_node = make_internal(root->children);
  sb.append("Extracted!");
}

void Extract::walk() {
  sb.append("NODE: ROOT");
  walk_root_node(root_node.get());
}

Node Extract::get_root() { return std::move(root_node); }

void Extract::walk_root_node(NodeBase *root) {
  if (root == nullptr) {
    return;
  }
  if (root->type == NodeType::Internal) {
    InternalBase *node = static_cast<InternalBase *>(root);
    for (auto it = node->children.begin(); it != node->children.end(); ++it) {
      sb.append("NODE: " + it->first);
      for (auto &item : it->second) {
        walk_root_node(item.get());
      }
    }
  } else {
    LeafBase *leaf = static_cast<LeafBase *>(root);
    if (leaf->content_idx == 0) {
      sb.append("TEXT: " + std::get<0>(leaf->content));
    } else {
      HTML::walk(std::get<1>(leaf->content).get());
    }
  }
}

Node Extract::make_leaf(xmlNode *cur_node) {
  if (cur_node == nullptr) {
    return nullptr;
  }

  xmlNode *node;
  Leaf leaf = std::make_unique<LeafBase>();
  for (node = cur_node; node; node = node->next) {
    if (node->type == XML_TEXT_NODE) {
      if (node->content) {
        leaf->content = std::string((char *)node->content);
        leaf->content_idx = 0;
      }
    } else if (node->type == XML_CDATA_SECTION_NODE) {
      if (node->content) {
        leaf->content = HTML::extract(node->content);
        leaf->content_idx = 1;
      }
    } else {
      sb.append(
          "XML Leaf node not a text node or cdata section; instead of type " +
          std::to_string(node->type));
      sb.append(xml_file + ": " + std::string((char *)node->name));
    }
  }

  return std::move(leaf);
}

Node Extract::make_internal(xmlNode *cur_node) {
  if (cur_node == nullptr) {
    return nullptr;
  }

  xmlNode *node;
  Internal node_obj = std::make_shared<InternalBase>();
  for (node = cur_node; node; node = node->next) {
    if (node->type == XML_ELEMENT_NODE) {
      std::string name{(const char *)(node->name)};
      Node internal_node =
          (name == "item" || name == "image" || name == "channel")
              ? make_internal(node->children)
              : make_leaf(node->children);
      node_obj->children[name].push_back(std::move(internal_node));
    }
  }
  return node_obj;
}

} // namespace XML
