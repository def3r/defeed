#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <curl/curl.h>

#include "fetcher.h"
#include "global.h"
#include "xml.h"

CurlGlobal::CurlGlobal() {}

void CurlGlobal::init() {
  static bool cinit = false;
  if (cinit == true) {
    return;
  }
  CURLcode result = curl_global_init(CURL_GLOBAL_ALL);
  if (result != CURLE_OK) {
    std::cerr << "Could not init curl" << std::endl;
    std::exit(1);
  }
  cinit = true;
}

void CurlGlobal::cleanup() {
  static bool ccleanup = false;
  if (ccleanup == true) {
    return;
  }
  curl_global_cleanup();
  ccleanup = true;
}

void DefeedCtx::PageUp() {
  if (DefeedCtx::page == DefeedCtx::Page::Landing)
    return;
  int pg = static_cast<int>(DefeedCtx::page);
  DefeedCtx::page = static_cast<DefeedCtx::Page>(pg - 1);
}

void DefeedCtx::PageDown() {
  if (DefeedCtx::page == DefeedCtx::Page::Blog)
    return;
  int pg = static_cast<int>(DefeedCtx::page);
  DefeedCtx::page = static_cast<DefeedCtx::Page>(pg + 1);
}

void DefeedCtx::init() {
  const char *env_home = std::getenv("HOME");
  if (env_home == nullptr) {
    std::cerr << "Env var $HOME not set" << std::endl;
    std::abort();
  }
  home = env_home;
  defeed = home + "/.defeed";
  rss = defeed + "/rss";
  rss_xml = rss + ".xml";
  rss_info = rss + ".info";
}

void DefeedCtx::setup_dirs() {
  namespace fs = std::filesystem;
  fs::path defeed_dir{DefeedCtx::defeed};
  if (!fs::is_directory(defeed_dir)) {
    std::cout << "defeed dir not found, initializing defeed." << std::endl;
    if (!fs::create_directory(defeed_dir)) {
      std::cerr << "Unable to create dir: " << defeed_dir << std::endl;
      std::exit(1);
    }
    if (!fs::create_directory(DefeedCtx::rss)) {
      std::cerr << "Unable to create dir: " << defeed_dir << std::endl;
      std::exit(1);
    }
    std::ofstream rss_file{DefeedCtx::rss_xml};
    rss_file.close();
  }
}

// TODO: this only reads from the rss.txt one time per launch, maybe have a cmd
// to force read it
DefeedCtx::UrlPairs DefeedCtx::get_urls() {
  static DefeedCtx::UrlPairs rss_urls{};
  if (rss_urls.size() != 0) {
    return rss_urls;
  }

  XML::Extract parse_xml(DefeedCtx::rss_xml);
  parse_xml.extract();
  XML::Node root = parse_xml.get_root();
  Walker w(root.get());

  XML::Internal r = XML_CAST_INTERNAL(root);
  if (r->children["category"].empty()) {
    sb.append("Category not found");
    return {};
  } else if (XML::LeafBase *child = static_cast<XML::LeafBase *>(
                 r->children["category"][0].get())) {
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
        XML_CAST_LEAF((XML_CAST_INTERNAL(items[i]))->children["link"][0]);
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
std::vector<std::shared_ptr<RssFeed>> DefeedCtx::fetchRSS() {
  DefeedCtx::UrlPairs rss_urls = DefeedCtx::get_urls();

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

  std::vector<std::shared_ptr<RssFeed>> nodes = DefeedCtx::loadRSS();

  return std::move(nodes);
}

// TODO: This is sequential for now but consider using async in future
std::vector<std::shared_ptr<RssFeed>> DefeedCtx::loadRSS() {
  std::vector<std::shared_ptr<RssFeed>> feeds;
  DefeedCtx::UrlPairs rss_urls = DefeedCtx::get_urls();

  for (auto [url, hash] : rss_urls) {
    const std::string dir_name{DefeedCtx::rss + "/" + std::to_string(hash)};
    const std::string file_name{dir_name + "/rssfeed.txt"};

    XML::Extract x{file_name};
    x.extract();

    std::unique_ptr<RssFeed> feed = std::make_unique<RssFeed>();
    feed->url = url;
    feed->hash = hash;
    feed->root = x.get_root();
    feeds.emplace_back(std::move(feed));
  }

  return std::move(feeds);
}

SharedBuffer::SharedBuffer() {
  buf.reserve(4096 * 8);
  entries.reserve(4096 * 8);
  lfile.open("log.txt", std::ios::out);
};

std::string SharedBuffer::getString() {
  std::unique_lock lock(mtx);
  return std::string(buf.begin(), buf.end());
}

void SharedBuffer::append(const std::string &s) {
  std::unique_lock lock(mtx);
  char *data = buf.data() + buf.size();
  buf.insert(buf.end(), s.begin(), s.end());
  entries.emplace_back(std::string_view(data, s.size()));
  lfile << s << std::endl;
  // TODO: buf sizecheck and invalidation on realloc
}

const std::vector<std::string_view> &SharedBuffer::getEntries() {
  std::unique_lock lock(mtx);
  return this->entries;
}

const std::size_t SharedBuffer::getTotalEntries() {
  std::unique_lock lock(mtx);
  return this->entries.size();
}

void SharedBuffer::lock() { mtx.lock(); }

void SharedBuffer::unlock() { mtx.unlock(); }
