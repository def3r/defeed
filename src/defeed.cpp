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
}

void setupDirs() {
  namespace fs = std::filesystem;
  fs::path defeed_dir{defeed};
  if (not fs::is_directory(defeed_dir)) {
    std::cout << "defeed dir not found, initializing defeed." << std::endl;
    if (not fs::create_directory(defeed_dir)) {
      std::cerr << "Unable to create dir: " << defeed_dir << std::endl;
      std::exit(1);
    }
    if (not fs::create_directory(rss)) {
      std::cerr << "Unable to create dir: " << defeed_dir << std::endl;
      std::exit(1);
    }
    std::ofstream rss_file{rss_xml};
    rss_file.close();
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
  Utils::Walker w(root.get());

  XML::Internal r = XML::cast_Internal(root);
  if (r->children["category"].empty()) {
    sb.append("Category not found");
    return {};
  } else if (XML::Leaf child = XML::cast_Leaf(r->children["category"][0])) {
    if (child->content_idx != 0) {
      sb.append("Expected a string but got HTML in <category>");
      return {};
    } else if (std::get<0>(child->content) != "rss") {
      sb.append("Expected rss but got " + std::get<0>(child->content) +
                " in <category>");
      return {};
    }
  }

  const XML::Siblings &items = r->children["item"];
  for (int i = 0; i < items.size(); i++) {
    XML::Leaf l =
        XML::cast_Leaf((XML::cast_Internal(items[i]))->children["rss"][0]);
    if (l->content_idx != 0) {
      sb.append("link not string but HTML");
      continue;
    }
    std::string url = std::get<0>(l->content);
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

std::vector<std::string> readToplevel() {
  std::vector<std::string> entries{};
  XML::Extract x{defeed_xml};
  x.extract();
  XML::Internal root = XML::cast_Internal(x.get_root());
  XML::Siblings &items = root->children["item"];
  for (XML::Node item : items) {
    Schema s{};
    XML::Internal i_item = XML::cast_Internal(item);
    assert(i_item->children.find("display") != i_item->children.end() &&
           "top level item must contain display");
    XML::Leaf leaf = XML::cast_Leaf(i_item->children["display"][0]);
    assert(leaf->content_idx == 0 &&
           "top level item display content must be string");
    std::string display = std::get<0>(leaf->content);
    s.put("display", display);

    assert((i_item->children.find("file") != i_item->children.end() ||
            i_item->children.find("link") != i_item->children.end()) &&
           "top level item must contain file or external link");
    if (i_item->children.find("file") != i_item->children.end()) {
      leaf = XML::cast_Leaf(i_item->children["file"][0]);
      assert(leaf->content_idx == 0 &&
             "top level item file content must be string");
      std::string filename = std::get<0>(leaf->content);
      s.put("file", filename);
    } else if (i_item->children.find("link") != i_item->children.end()) {
      leaf = XML::cast_Leaf(i_item->children["link"][0]);
      assert(leaf->content_idx == 0 &&
             "top level item link content must be string");
      std::string link = std::get<0>(leaf->content);
      s.put("link", link);
    }

    toplevel.push_back(s);
  }
  return entries;
}

} // namespace DefeedCtx
