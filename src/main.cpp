#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include <curl/curl.h>
#include <curl/multi.h>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <libxml/HTMLparser.h>
#include <libxml/HTMLtree.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "fetcher.h"
#include "global.h"
#include "libxml/xmlmemory.h"
#include "libxml/xmlstring.h"

static void print_element_names(xmlNode *a_node) {
  xmlNode *cur_node = NULL;

  for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if (cur_node->ns != nullptr) {
        std::cout << "Namespace: " << cur_node->ns->prefix << std::endl;
      }
      printf("node type: Element, name: %s\n", cur_node->name);
    } else if (cur_node->type == XML_CDATA_SECTION_NODE) {
      std::cout << "TEXT: " << cur_node->content;
      std::abort();
    } else {
      std::cout << "NODE NAME : " << cur_node->name << "\t" << cur_node->content
                << std::endl;
    }
    print_element_names(cur_node->children);
  }
}

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

  Node() {
    this->type = NodeType::Internal;
    this->attrs = {};
  }

  // TODO: Here Do we need to destroy the children explicitly?
};

struct Leaf : public NodeBase {
  std::string content;

  Leaf() {
    this->type = NodeType::Leaf;
    this->attrs = {};
  }
};

// The only leaf node?
using TEXT = Leaf;

static std::unique_ptr<HTML::NodeBase> make_leaf(xmlNode *cur_node) {
  if (cur_node == nullptr) {
    return nullptr;
  }
  std::unique_ptr<HTML::Leaf> leaf = std::make_unique<HTML::Leaf>();
  if (cur_node->type == HTML_TEXT_NODE && cur_node->content) {
    std::cout << "\t\tTEXT: " << cur_node->content << ": "
              << std::strlen((char *)cur_node->content) << std::endl;
    std::string content((char *)cur_node->content);
    if (content.find_first_not_of(" \t\n\r") == std::string::npos ||
        content.empty()) {
      return {};
    }
    leaf->content = std::string((char *)cur_node->content);
    leaf->attrs["HTMLNodeName"] = "text";
  }

  return std::move(leaf);
}

static std::unique_ptr<HTML::NodeBase> make_internal(xmlNode *cur_node) {
  if (cur_node == nullptr) {
    return nullptr;
  }
  if (cur_node->type == XML_TEXT_NODE) {
    std::cout << "make_internal: leaf node passed, requesting make_leaf"
              << std::endl;
    return make_leaf(cur_node);
  }
  if (cur_node->type != XML_ELEMENT_NODE) {
    std::cout << "make_internal: node not an XML_ELEMENT_NODE" << std::endl;
    return nullptr;
  }

  // TODO: skip html and body tags; we are not interested in storing them
  std::string cur_node_name{(char *)cur_node->name};
  // if (cur_node_name == "html" || cur_node_name == "body") {
  //   return make_internal(cur_node->children);
  // }

  std::unique_ptr<HTML::Node> node_obj = std::make_unique<HTML::Node>();
  node_obj->attrs["HTMLNodeName"] = cur_node_name;
  xmlAttrPtr attrs = cur_node->properties;
  for (; attrs; attrs = attrs->next) {
    if (attrs->name) {
      xmlChar *val = xmlGetProp(cur_node, attrs->name);
      if (val == nullptr) {
        std::cout << attrs->name << " << noVAL " << std::endl;
        node_obj->attrs[std::string((char *)attrs->name)] = "";
      } else {
        std::cout << attrs->name << " << " << val << std::endl;
        node_obj->attrs[std::string((char *)attrs->name)] =
            std::string((char *)val);
      }
      xmlFree(val);
    }
  }
  if (cur_node->content) {
    std::cout << "make_internal: Internal node with content: "
              << cur_node->content << std::endl;
  }

  xmlNode *node = nullptr;
  std::unique_ptr<NodeBase> child;
  for (node = cur_node->children; node; node = node->next) {
    if (node->type == XML_TEXT_NODE) {
      child = make_leaf(node);
      if (child != nullptr) {
        node_obj->children.emplace_back(std::move(child));
      }
    } else if (node->type == XML_ELEMENT_NODE) {
      child = make_internal(node);
      if (child != nullptr) {
        node_obj->children.emplace_back(std::move(make_internal(node)));
      }
    }
  }

  return std::move(node_obj);
}

std::unique_ptr<HTML::NodeBase> extract(const xmlChar *str) {
  if (str == nullptr) {
    std::cout << "HTML::extract : passed nullptr for in memory str"
              << std::endl;
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
  std::unique_ptr<NodeBase> root_node = make_internal(root);

  xmlFreeDoc(doc);

  return std::move(root_node);
}

void walk(HTML::NodeBase *root) {
  if (root == nullptr) {
    return;
  }
  std::cout << "\t\tHTML::" << root->attrs["HTMLNodeName"] << std::endl;
  for (auto it = root->attrs.begin(); it != root->attrs.end(); ++it) {
    if (it->first != "HTMLNodeName")
      std::cout << "\t\t\t" << it->first << " = " << it->second << std::endl;
  }

  if (root->type == HTML::NodeType::Leaf) {
    HTML::Leaf *leaf = static_cast<HTML::Leaf *>(root);
    std::cout << "\t\tContents: " << leaf->content << std::endl;
  } else {
    HTML::Node *node = static_cast<HTML::Node *>(root);
    for (int i = 0; i < node->children.size(); i++) {
      HTML::NodeBase *child = node->children[i].get();
      walk(child);
    }
  }
}

} // namespace HTML
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

  Node() {
    this->type = NodeType::Internal;
    this->attrs = {};
  }

  // TODO: Here Do we need to destroy the children explicitly?
};

struct Leaf : public NodeBase {
  std::variant<std::string, std::unique_ptr<HTML::NodeBase>> content;
  int content_idx;

  Leaf() {
    this->type = NodeType::Leaf;
    this->attrs = {};
    content_idx = 0;
  }
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
  ~Extract() {
    if (doc != nullptr) {
      xmlFreeDoc(doc);
    }
  }

  Extract(const std::string &xml_file) {
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
  Extract(Extract &&other) {
    this->doc = other.doc;
    this->root = other.root;
    this->root_node = std::move(other.root_node);

    other.doc = nullptr;
    other.root = nullptr;
    other.root_node = nullptr;
  }
  // move assignment
  Extract &operator=(Extract &&other) {
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

  Extract(const Extract &) = delete;
  Extract &operator=(const Extract &) = delete;

  // Calling function assign
  // Called  function allocates
  void extract() {
    root_node = make_internal(root->children);
    std::cout << "Extracted!" << std::endl;
  }

  void walk() {
    std::cout << "NODE: ROOT" << std::endl;
    walk_root_node(root_node.get());
  }

  std::unique_ptr<NodeBase> get_root() { return std::move(root_node); }

private:
  xmlDoc *doc;
  xmlNode *root;
  std::unique_ptr<NodeBase> root_node;

  void walk_root_node(NodeBase *root) {
    if (root == nullptr) {
      return;
    }
    if (root->type == NodeType::Internal) {
      Node *node = static_cast<Node *>(root);
      for (auto it = node->children.begin(); it != node->children.end(); ++it) {
        std::cout << "NODE: " << it->first << std::endl;
        for (auto &item : it->second) {
          walk_root_node(item.get());
        }
      }
    } else {
      Leaf *leaf = static_cast<Leaf *>(root);
      if (leaf->content_idx == 0) {
        std::cout << "TEXT: " << std::get<0>(leaf->content) << std::endl;
      } else {
        HTML::walk(std::get<1>(leaf->content).get());
      }
    }
  }

  std::unique_ptr<NodeBase> make_leaf(xmlNode *cur_node) {
    if (cur_node == nullptr) {
      return nullptr;
    }

    xmlNode *node;
    std::unique_ptr<Leaf> leaf = std::make_unique<Leaf>();
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
        std::cout
            << "XML Leaf node not a text node or cdata section; instead a "
            << node->type << std::endl;
      }
    }

    return std::move(leaf);
  }

  std::unique_ptr<NodeBase> make_internal(xmlNode *cur_node) {
    if (cur_node == nullptr) {
      return nullptr;
    }

    xmlNode *node;
    std::unique_ptr<Node> node_obj = std::make_unique<Node>();
    for (node = cur_node; node; node = node->next) {
      if (node->type == XML_ELEMENT_NODE) {
        std::string name{(const char *)(node->name)};
        std::unique_ptr<NodeBase> internal_node =
            (name == "item" || name == "image" || name == "channel")
                ? make_internal(node->children)
                : make_leaf(node->children);
        node_obj->children[name].push_back(std::move(internal_node));
      }
    }
    return node_obj;
  }
};

} // namespace XML

int main(int argc, char *argv[]) {
  using namespace ftxui;

  CurlGlobal::init();
  DefeedCtx::init();
  DefeedCtx::setup_dirs();

  std::ifstream rss_txt{DefeedCtx::rss_txt};
  if (!rss_txt.is_open()) {
    std::cerr << "Cant open file " << DefeedCtx::rss_txt << std::endl;
    std::exit(1);
  }

  std::vector<std::pair<std::string, size_t>> rss_urls{};
  while (!rss_txt.eof()) {
    std::string url{};
    std::getline(rss_txt, url);
    if (!url.empty()) {
      size_t hash = std::hash<std::string>{}(url);
      std::cout << hash << "\t\t" << url << std::endl;
      rss_urls.push_back({url, hash});
    }
  }
  rss_txt.close();

  MultiFetcher fetch_rss{};
  MultiFetcher cond_fetch_rss{};
  for (auto [url, hash] : rss_urls) {
    std::filesystem::path url_path{DefeedCtx::rss + "/" + std::to_string(hash)};
    if (!std::filesystem::is_directory(url_path)) {
      fetch_rss.add(url);
      continue;
    }
    std::ifstream etag_file{url_path.string() + "/etag"};
    if (!etag_file.is_open()) {
      std::cout << "Can't find etag file for " << hash << " (" << url << ")"
                << std::endl;
      std::cout << "\t" << "Fetching feed for " << hash << " (" << url << ")"
                << std::endl;
      fetch_rss.add(url);
      continue;
    }
    std::string etag{};
    etag_file >> etag;
    etag_file.close();

    std::cout << "If-None-Match: " << etag << std::endl;

    Fetcher f{url, etag};
    f.append_headers("If-None-Match: " + etag);
    f.apply_headers();
    cond_fetch_rss.add(std::move(f));
  }

  fetch_rss.perform_write();
  cond_fetch_rss.perform_write();

  for (auto [url, hash] : rss_urls) {
    const std::string dir_name{DefeedCtx::rss + "/" + std::to_string(hash)};
    const std::string file_name{dir_name + "/rssfeed.txt"};

    XML::Extract x{file_name};
    x.extract();
    x.walk();

    break;
  }

  // Create a simple document with three text elements.
  Element document = hbox({
      text("left") | border,
      text("middle") | border | flex,
      text("right") | border,
  });

  // Create a screen with full width and height fitting the document.
  auto screen = Screen::Create(Dimension::Full(),       // Width
                               Dimension::Fit(document) // Height
  );

  // Render the document onto the screen.
  Render(screen, document);

  // Print the screen to the console.
  screen.Print();

  CurlGlobal::cleanup();
}
