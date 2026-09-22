#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <vector>

#include <curl/curl.h>

#include "fetcher.h"
#include "global.h"

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

void DefeedCtx::init() {
  const char *env_home = std::getenv("HOME");
  if (env_home == nullptr) {
    std::cerr << "Env var $HOME not set" << std::endl;
    std::abort();
  }
  home = env_home;
  defeed = home + "/.defeed";
  rss = defeed + "/rss";
  rss_txt = rss + ".txt";
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
    std::ofstream rss_file{DefeedCtx::rss_txt};
    rss_file.close();
  }
}

// TODO: This is increasing couplic for global.cpp, maybe move somewhere else
std::vector<std::unique_ptr<XML::NodeBase>> DefeedCtx::fetchRSS() {
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
      sb.append(std::to_string(hash) + "\t\t" + url);
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

  std::vector<std::unique_ptr<XML::NodeBase>> nodes;

  for (auto [url, hash] : rss_urls) {
    const std::string dir_name{DefeedCtx::rss + "/" + std::to_string(hash)};
    const std::string file_name{dir_name + "/rssfeed.txt"};

    XML::Extract x{file_name};
    x.extract();

    nodes.emplace_back(x.get_root());
  }

  return nodes;
}

SharedBuffer::SharedBuffer() {
  buf.reserve(4096);
  entries.reserve(4096);
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
