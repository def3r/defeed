#include <cstring>
#include <iostream>

#include <libxml/HTMLparser.h>
#include <libxml/HTMLtree.h>

#include "global.h"
#include "html.h"
#include "sb.h"

namespace HTML {

InternalBase::InternalBase() {
  this->type = NodeType::Internal;
  this->attrs = {};
}

LeafBase::LeafBase() {
  this->type = NodeType::Leaf;
  this->attrs = {};
}

static Node makeLeaf(xmlNode *cur_node) {
  if (cur_node == nullptr) {
    return nullptr;
  }
  Leaf leaf = std::make_shared<HTML::LeafBase>();
  if (cur_node->type == HTML_TEXT_NODE and cur_node->content) {
    // std::cout << "\t\tTEXT: " << cur_node->content << ": "
    //           << std::strlen((char *)cur_node->content) << std::endl;
    std::string content((char *)cur_node->content);
    if (content.find_first_not_of(" \t\n\r") == std::string::npos or
        content.empty()) {
      return {};
    }
    leaf->content = std::string((char *)cur_node->content);
    leaf->attrs["HTMLNodeName"] = "text";
  }

  return std::move(leaf);
}

static Node makeInternal(xmlNode *cur_node) {
  if (cur_node == nullptr) {
    return nullptr;
  }
  if (cur_node->type == XML_TEXT_NODE) {
    // std::cout << "make_internal: leaf node passed, requesting make_leaf"
    //           << std::endl;
    return makeLeaf(cur_node);
  }
  if (cur_node->type != XML_ELEMENT_NODE) {
    sb.append("make_internal: node not an XML_ELEMENT_NODE");
    return nullptr;
  }

  // TODO: skip html and body tags; we are not interested in storing them
  std::string cur_node_name{(char *)cur_node->name};
  // if (cur_node_name == "html" || cur_node_name == "body") {
  //   return make_internal(cur_node->children);
  // }

  Internal node_obj = std::make_shared<HTML::InternalBase>();
  node_obj->attrs["HTMLNodeName"] = cur_node_name;
  xmlAttrPtr attrs = cur_node->properties;
  for (; attrs; attrs = attrs->next) {
    if (attrs->name) {
      xmlChar *val = xmlGetProp(cur_node, attrs->name);
      if (val == nullptr) {
        // std::cout << attrs->name << " << noVAL " << std::endl;
        node_obj->attrs[std::string((char *)attrs->name)] = "";
      } else {
        // std::cout << attrs->name << " << " << val << std::endl;
        node_obj->attrs[std::string((char *)attrs->name)] =
            std::string((char *)val);
      }
      xmlFree(val);
    }
  }
  if (cur_node->content) {
    sb.append("make_internal: Internal node with content: " +
              std::string((char *)cur_node->content));
  }

  xmlNode *node = nullptr;
  Node child;
  for (node = cur_node->children; node; node = node->next) {
    if (node->type == XML_TEXT_NODE) {
      child = makeLeaf(node);
      if (child != nullptr) {
        node_obj->children.emplace_back(std::move(child));
      }
    } else if (node->type == XML_ELEMENT_NODE) {
      child = makeInternal(node);
      if (child != nullptr) {
        node_obj->children.emplace_back(std::move(makeInternal(node)));
      }
    }
  }

  return std::move(node_obj);
}

Node extract(const xmlChar *str) {
  if (str == nullptr) {
    sb.append("HTML::extract : passed nullptr for in memory str");
    return {};
  }

  const char *cstr = (const char *)str;
  xmlDoc *doc = htmlReadMemory(cstr, std::strlen(cstr), NULL, "UTF-8",
                               HTML_PARSE_NOBLANKS);
  if (doc == nullptr) {
    std::cerr << "HTML::extract : "
              << "Unable to parse in memory CDATA section as html.\nDATA: "
              << cstr << std::endl;
    std::abort();
  }
  xmlNode *root = xmlDocGetRootElement(doc);
  Node root_node = makeInternal(root);

  xmlFreeDoc(doc);

  return std::move(root_node);
}

void walk(HTML::NodeBase *root) {
  if (root == nullptr) {
    return;
  }
  sb.append("\t\tHTML::" + root->attrs["HTMLNodeName"]);
  for (auto it = root->attrs.begin(); it != root->attrs.end(); ++it) {
    if (it->first != "HTMLNodeName")
      sb.append("\t\t\t" + it->first + " = " + it->second);
  }

  if (root->type == HTML::NodeType::Leaf) {
    HTML::LeafBase *leaf = static_cast<HTML::LeafBase *>(root);
    sb.append("\t\tContents: " + leaf->content);
  } else {
    HTML::InternalBase *node = static_cast<HTML::InternalBase *>(root);
    for (int i = 0; i < node->children.size(); i++) {
      HTML::NodeBase *child = node->children[i].get();
      walk(child);
    }
  }
}

} // namespace HTML
