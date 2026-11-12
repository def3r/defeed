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

void Schema::put(const std::string &key, const std::string &val) {
  this->m[key] = val;

  Schema::fieldEncodeTy::iterator it;
  if ((it = Schema::fieldEncode.find(key)) == Schema::fieldEncode.end()) {
    this->fields |= Schema::Field::OTHER;
  } else {
    this->fields |= it->second;
  }
}

bool Schema::find(const std::string &key) const {
  if (this->m.find(key) == this->m.end())
    return false;
  return true;
}

std::vector<std::string> Schema::getAll(const Schemas &ss,
                                        const std::string &key) {
  std::vector<std::string> entries{};
  for (const Schema &s : ss) {
    std::unordered_map<std::string, std::string>::const_iterator it =
        s.m.find(key);

    if (it == s.m.end())
      continue;

    entries.push_back(it->second);
  }
  return entries;
}

bool Schema::hasPage() { return pid != -1; }

PageID PCB::create(const Schemas &ss) {
  PageID pid = PCB::pctx.size();
  PCB::pctx.push_back((PageCtx){.pid = pid, .ss = ss});
  return pid;
}
PageID PCB::create(const Schemas &ss, const std::string &entry_key) {
  PageID pid = PCB::pctx.size();
  PCB::pctx.push_back((PageCtx){.pid = pid, .ss = ss, .entry_key = entry_key});
  return pid;
}

PageID PCB::create(XML::Node root, const std::string &entry_key,
                   const std::vector<std::string> &entries) {
  PageID pid = PCB::pctx.size();
  PCB::pctx.push_back((PageCtx){
      .pid = pid,
      .ss = {},
      .root = root,
      .entry_key = entry_key,
      .entries = entries,
  });
  return pid;
}

PageCtx &PCB::getRefTo(const PageID pid) {
  assert(pid < PCB::pctx.size() && "getRefTo: pid > PCB size");
  return PCB::pctx[pid];
}
Schema &PCB::getSchemaAt(const PageID pid, const size_t index) {
  assert(pid < PCB::pctx.size() && "getSchemaAt: pid > PCB size");
  assert(index < PCB::pctx[pid].ss.size() && "getSchemaAt: index > schema set");
  return PCB::pctx[pid].ss[index];
}
PageID PCB::createForSchema(const PageID pid, const size_t index) {
  assert(pid < PCB::pctx.size() && "createFromSchema: pid > PCB size");
  assert(index < PCB::pctx[pid].ss.size() &&
         "createFromSchema: index > schema set");
  return 0;
}

void DefeedCtx::init() {
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

// TODO: this only reads from the rss.xml one time per launch, maybe have a cmd
// to force read it
DefeedCtx::UrlPairs DefeedCtx::get_urls() {
  // The first time we get urls, we must populate the schema
  static DefeedCtx::UrlPairs rss_urls{};
  if (rss_urls.size() != 0) {
    return rss_urls;
  }

  XML::Extract parse_xml{DefeedCtx::rss_xml};
  parse_xml.extract();
  XML::Node root = parse_xml.get_root();
  Walker w(root.get());

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
    Schema s{};

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

    // TODO: populate other tags into schema as well
    s.put("rss", url);
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

XML::Node DefeedCtx::loadRSS(const std::string &url) {
  size_t hash = std::hash<std::string>{}(url);
  std::filesystem::path url_path{DefeedCtx::rss + "/" + std::to_string(hash)};
  Fetcher fetch_rss{};
  // NOTE: We dont update each time a load request is made; to update the rss,
  // explicitly request the fetchRSS
  if (!std::filesystem::is_directory(url_path)) {
    MultiFetcher fetch_rss{};
    fetch_rss.add(url);
    sb.append("Fetching " + url);
    if (!fetch_rss.perform_write()) {
      sb.append("Fetching " + url + " failed; Aborting");
      return nullptr;
    }
  }
  const std::string dir_name{DefeedCtx::rss + "/" + std::to_string(hash)};
  const std::string file_name{dir_name + "/rssfeed.txt"};

  XML::Extract x{file_name};
  x.extract();
  return x.get_root();
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

std::vector<std::string> DefeedCtx::read_toplevel() {
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

SharedBuffer::SharedBuffer() {
  buf.reserve(4096 * 8);
  entries.reserve(4096 * 8);
  lfile.open("log.txt", std::ios::out);
};

std::string SharedBuffer::getString() {
  std::unique_lock lock{mtx};
  return std::string(buf.begin(), buf.end());
}

void SharedBuffer::append(const std::string &s) {
  std::unique_lock lock{mtx};
  char *data = buf.data() + buf.size();
  buf.insert(buf.end(), s.begin(), s.end());
  entries.emplace_back(std::string_view(data, s.size()));
  lfile << s << std::endl;
  // TODO: buf sizecheck and invalidation on realloc
}

const std::vector<std::string_view> &SharedBuffer::getEntries() {
  std::unique_lock lock{mtx};
  return this->entries;
}

const std::size_t SharedBuffer::getTotalEntries() {
  std::unique_lock lock{mtx};
  return this->entries.size();
}

void SharedBuffer::lock() { mtx.lock(); }

void SharedBuffer::unlock() { mtx.unlock(); }
