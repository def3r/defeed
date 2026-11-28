#include <filesystem>
#include <iostream>

#include "defeed.h"
#include "fetcher.h"
#include "global.h"
#include "page.h"
#include "sb.h"

namespace DefeedCtx {

void init() {
  const char *env_home = std::getenv("HOME");
  if (env_home == nullptr) {
    std::cerr << "Env var $HOME not set" << std::endl;
    std::abort();
  }
  // clang-format off
  home       = env_home;
  defeed     = home   + "/.defeed/";
  rss        = defeed + "rss";
  rss_xml    = rss    + ".xml";
  rss_info   = rss    + ".info";
  defeed_xml = defeed + "defeed.xml";
  // clang-format on

  PCB::init();
}

void setupDirs() {
  namespace fs = std::filesystem;
  fs::path defeed_dir{defeed};
  if (not fs::is_directory(defeed_dir)) {
    std::cout << "defeed dir not found, initializing defeed." << std::endl;
    sb.append("defeed dir not found, initializing defeed.");
    if (not fs::create_directory(defeed_dir)) {
      std::cerr << "Unable to create dir: " << defeed_dir << std::endl;
      std::exit(1);
    }
    if (not fs::create_directory(rss)) {
      std::cerr << "Unable to create dir: " << rss << std::endl;
      std::exit(1);
    }
    std::ofstream rss_file{rss_xml};
    rss_file.close();

    std::ofstream file{rss_xml};
    file.close();

    XML::Root root = XML::Create::Root();
    XML::Leaf ctgy = XML::Create::Leaf("category", "defeedtoplevel");
    XML::Leaf disp =
        XML::Create::Leaf("display", "Welcome to Defeeed, Press enter to "
                                     "checkout how to create the config!");
    XML::Leaf link = XML::Create::Leaf(
        "link", "https://github.com/def3r/defeed/blob/main/readme.txt");
    root << ctgy << XML::Create::Internal("item", {disp, link});
    XML::Serialize(defeed_xml, root);
  }
}

// TODO: this only reads from the rss.xml one time per launch, maybe have a cmd
// to force read it
UrlPairs getURLs() {
  // The first time we get urls, we must populate the schema
  static UrlPairs rss_urls{};
  if (rss_urls.size() != 0) {
    return rss_urls;
  }

  XML::Extract parse_xml{rss_xml};
  parse_xml.extract();
  XML::Node root = parse_xml.get_root();
  Walker w(root.get());

  XML::Internal r = XML::cast_Internal(root);
  if (not r->contains("category")) {
    sb.append("Category not found");
    return {};
  } else if (XML::Leaf child = XML::cast_Leaf(r->children["category"][0])) {
    std::string val{};
    if (child->is_content_string()) {
      sb.append("Expected a string but got HTML in <category>");
      return {};
    } else if ((val = child->get_content_string()) != "rss") {
      sb.append("Expected rss but got " + val + " in <category>");
      return {};
    }
  }

  const XML::Siblings &items = r->children["item"];
  for (XML::Node item : items) {
    XML::Leaf l =
        XML::cast_Leaf((XML::cast_Internal(item))->children["rss"][0]);
    if (l->is_content_string()) {
      sb.append("link not string but HTML");
      continue;
    }
    std::string url = l->get_content_string();
    size_t hash = std::hash<std::string>{}(url);
    sb.append(std::to_string(hash) + "\t\t" + url);
    rss_urls.push_back({url, hash});
  }

  return rss_urls;
}

// TODO: This is increasing couplic for global.cpp, maybe move somewhere else
void fetchRSS() {
  UrlPairs rss_urls = getURLs();

  MultiFetcher fetch_rss{};
  MultiFetcher cond_fetch_rss{};
  for (auto [url, hash] : rss_urls) {
    std::filesystem::path url_path{rss + "/" + std::to_string(hash)};
    if (not std::filesystem::is_directory(url_path)) {
      fetch_rss.add(url);
      continue;
    }
    std::ifstream etag_file{url_path.string() + "/etag"};
    if (not etag_file.is_open()) {
      sb.append("Can't find etag file for " + std::to_string(hash) + " (" +
                url + ")");
      sb.append("\tFetching feed for " + std::to_string(hash) + " (" + url +
                ")");
      fetch_rss.add(url);
      continue;
    }
    std::string etag{};
    etag_file >> etag;
    etag_file.close();

    sb.append("If-None-Match: " + etag);

    Fetcher f{url, etag};
    f.append_headers("If-None-Match: " + etag);
    f.apply_headers();
    cond_fetch_rss.add(std::move(f));
  }

  fetch_rss.perform_write();
  cond_fetch_rss.perform_write();
  sb.append("Completed fetches!");
}

XML::Node loadRSS(const std::string &url) {
  size_t hash = std::hash<std::string>{}(url);
  std::filesystem::path url_path{rss + "/" + std::to_string(hash)};
  Fetcher fetch_rss{};
  // NOTE: We dont update each time a load request is made; to update the rss,
  // explicitly request the fetchRSS
  if (not std::filesystem::is_directory(url_path)) {
    MultiFetcher fetch_rss{};
    fetch_rss.add(url);
    sb.append("Fetching " + url);
    if (not fetch_rss.perform_write()) {
      sb.append("Fetching " + url + " failed; Aborting");
      return nullptr;
    }
  }
  const std::string dir_name{rss + "/" + std::to_string(hash)};
  const std::string file_name{dir_name + "/rssfeed.txt"};

  XML::Extract x{file_name};
  x.extract();
  return x.get_root();
}

Strings readToplevel() {
  Strings entries{};
  XML::Extract x{defeed_xml};
  x.extract();
  XML::Internal root = XML::cast_Internal(x.get_root());
  XML::Siblings &items = root->children["item"];
  for (XML::Node item : items) {
    Schema s{};
    XML::Internal i_item = XML::cast_Internal(item);
    XML::mustContain(i_item->children, "display");
    XML::Leaf leaf = XML::cast_Leaf(i_item->children["display"][0]);
    s.put("display", leaf->get_content_string());

    std::string key = "";
    if (i_item->contains("file")) {
      key = "file";
    } else if (i_item->contains("link")) {
      key = "link";
    } else {
      Utils::Assert(false, "top level item must contain file or external link");
    }
    leaf = XML::cast_Leaf(i_item->children[key][0]);
    s.put(key, leaf->get_content_string());

    toplevel.push_back(s);
  }
  return entries;
}

Walker::Walker(XML::NodeBase *_Nonnull feed) : root(feed) {
  fs.open(filename, std::ios::out);
  fs << "[ROOT]\n";
  walk_internal(0, (XML::InternalBase *)root);
  fs.close();
}

void Walker::walk_internal(int level, XML::InternalBase *root) {
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

void Walker::walk_leaf(int level, XML::LeafBase *root) {
  if (root == nullptr)
    return;

  std::string indent(level, ' ');
  if (root->content_idx == 0) {
    fs << indent << "[TEXT]: " << std::get<0>(root->content) << std::endl;
  } else {
    fs << indent << "[HTML]: " << std::endl;
  }
}

void ConfigWriter::export_internal(std::ofstream &fs, XML::Internal root,
                                   std::stack<std::string> &fstack,
                                   const std::string &leaf_separator) {
  for (auto &[tag, sibs] : root->children) {
    if (sibs.empty()) {
      continue;
    }
    if (sibs[0]->type == XML::NodeType::Leaf) {
      for (auto sib : sibs) {
        XML::Leaf leaf = std::static_pointer_cast<XML::LeafBase>(sib);
        // TODO: Add html support
        // TODO: Serialize the leaf string! (infact all strings), and also the &
        // -> &amp; thingy
        if (leaf_separator == "\n") {
          fs << "    << ";
        }
        fs << "XML::Create::Leaf(\"" << tag << "\", \""
           << leaf->get_content_string() << "\")" << leaf_separator;
        if (tag == "file") {
          fstack.push(defeed + leaf->get_content_string());
        }
      }
    } else if (sibs[0]->type == XML::NodeType::Internal) {
      for (auto sib : sibs) {
        fs << "    << XML::Create::Internal(\"" << tag << "\", {";
        export_internal(fs, std::static_pointer_cast<XML::InternalBase>(sib),
                        fstack, ", ");
        fs << "})\n";
      }
    }
  }
}

// TODO: instead of using DefeedCtx::defeed while exporting, use it while
// importing
void ConfigWriter::export_config() {
  std::ofstream fs{fout, std::ios_base::out | std::ios_base::trunc};
  fs << export_file_content_prologue;

  std::stack<std::string> fstack{};
  fstack.push(DefeedCtx::defeed_xml);
  while (not fstack.empty()) {
    std::string fname{fstack.top()};
    fstack.pop();
    XML::Extract x{fname};
    x.extract();
    auto root = x.get_root();

    fs << "\n  root = XML::Create::Root();\n";
    fs << "  XML::Serialize(\"" << fname << "\", root\n";
    export_internal(fs, std::static_pointer_cast<XML::InternalBase>(root),
                    fstack, "\n");
    fs << "  );\n";
  }

  fs << export_file_content_epilogue;
}

} // namespace DefeedCtx
