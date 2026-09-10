#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <curl/curl.h>
#include <curl/multi.h>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "fetcher.h"
#include "global.h"

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
  std::string text;

  Leaf() {
    this->type = NodeType::Leaf;
    this->attrs = {};
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

} // namespace XML

class XMLExtract {
public:
  XMLExtract() = delete;
  ~XMLExtract() {
    if (doc != nullptr) {
      xmlFreeDoc(doc);
    }
  }

  XMLExtract(const std::string &xml_file) {
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
  XMLExtract(XMLExtract &&other) {
    this->doc = other.doc;
    this->root = other.root;
    this->root_node = std::move(other.root_node);

    other.doc = nullptr;
    other.root = nullptr;
    other.root_node = nullptr;
  }
  // move assignment
  XMLExtract &operator=(XMLExtract &&other) {
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

  XMLExtract(const XMLExtract &) = delete;
  XMLExtract &operator=(const XMLExtract &) = delete;

  // Calling function assign
  // Called  function allocates
  void extract() {
    root_node = traverse(root->children);
    std::cout << "Extracted!" << std::endl;
  }

  void walk() {
    std::cout << "NODE: ROOT" << std::endl;
    walk_root_node(root_node.get());
  }

private:
  xmlDoc *doc;
  xmlNode *root;
  std::unique_ptr<XML::NodeBase> root_node;

  void walk_root_node(XML::NodeBase *root) {
    if (root == nullptr) {
      return;
    }
    if (root->type == XML::NodeType::Internal) {
      XML::Node *node = static_cast<XML::Node *>(root);
      for (auto it = node->children.begin(); it != node->children.end(); ++it) {
        std::cout << "NODE: " << it->first << std::endl;
        for (auto &item : it->second) {
          walk_root_node(item.get());
        }
      }
    } else {
      XML::Leaf *leaf = static_cast<XML::Leaf *>(root);
      std::cout << "TEXT: " << leaf->text << std::endl;
    }
  }

  std::unique_ptr<XML::NodeBase> make_leaf(xmlNode *cur_node) {
    if (cur_node == nullptr) {
      return nullptr;
    }

    xmlNode *node;
    std::unique_ptr<XML::Leaf> node_obj = std::make_unique<XML::Leaf>();
    for (node = cur_node; node; node = node->next) {
      if (node->type == XML_TEXT_NODE || node->type == XML_CDATA_SECTION_NODE) {
        if (node->content) {
          node_obj->text = std::string((char *)node->content);
        }
      } else {
        std::cout
            << "XML Leaf node not a text node or cdata section; instead a "
            << node->type << std::endl;
      }
    }

    return std::move(node_obj);
  }

  std::unique_ptr<XML::NodeBase> traverse(xmlNode *cur_node) {
    if (cur_node == nullptr) {
      return nullptr;
    }

    xmlNode *node;
    std::unique_ptr<XML::Node> node_obj = std::make_unique<XML::Node>();
    for (node = cur_node; node; node = node->next) {
      if (node->type == XML_ELEMENT_NODE) {
        std::string name{(const char *)(node->name)};
        std::unique_ptr<XML::NodeBase> internal_node;
        if (name == "item" || name == "image" || name == "channel") {
          internal_node = traverse(node->children);
        } else {
          internal_node = make_leaf(node->children);
        }
        node_obj->children[name].push_back(std::move(internal_node));
      }
    }
    return node_obj;
  }
};

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

    XMLExtract x{file_name};
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
