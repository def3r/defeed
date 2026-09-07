#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <curl/curl.h>

#include "fetcher.h"
#include "global.h"

class MultiFetcher;

Fetcher::Fetcher() { init_common(); }

// RAII obj, steal on move, delete on copy
// move constructor
Fetcher::Fetcher(Fetcher &&other) noexcept {
  this->curl = other.curl;
  this->url = other.url;
  this->headers = other.headers;

  other.curl = nullptr;
  other.headers = nullptr;
}

// move assignment
Fetcher &Fetcher::operator=(Fetcher &&other) noexcept {
  if (this != &other) {
    this->curl = other.curl;
    this->url = other.url;
    this->headers = other.headers;

    other.curl = nullptr;
    other.headers = nullptr;
  }
  return *this;
}

Fetcher::Fetcher(const std::string &url) : url(url) {
  init_common();
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
}

Fetcher::~Fetcher() {
  if (curl) {
    curl_easy_cleanup(curl);
  }
  if (headers) {
    curl_slist_free_all(headers);
  }
}

CURLcode Fetcher::perform_write(const std::string &fname) {
  FILE *pagefile = fopen(fname.c_str(), "wb");
  if (!pagefile) {
  }
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, pagefile);
  res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    fprintf(stderr, "curl_easy_perform() failed: %s\n",
            curl_easy_strerror(res));
    return res;
  }
  fclose(pagefile);
  return res;
}

void Fetcher::append_headers(const std::string &header_str) {
  headers = curl_slist_append(headers, header_str.c_str());
}

void Fetcher::apply_headers() {
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
}

size_t Fetcher::write_cb(char *ptr, size_t size, size_t nmemb, void *stream) {
  size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
  return written;
}

size_t Fetcher::header_callback(char *buffer, size_t size, size_t nitems,
                                void *userdata) {
  size_t n = size * nitems;

  if (userdata == nullptr)
    return n;
  size_t hash = *static_cast<size_t *>(userdata);

  char temp[n - 1]; // ignore \r\n
  std::memcpy(temp, buffer, n - 2);
  temp[n - 2] = '\0';
  std::string res{temp};

  size_t pos = res.find("etag: ");
  if (pos != std::string::npos) {
    std::filesystem::path url_path{DefeedCtx::rss + "/" + std::to_string(hash)};
    // At this point we assume all the dirs have been setup
    if (!std::filesystem::is_directory(url_path)) {
      std::abort();
    }
    std::ofstream etag_file{url_path.string() + "/etag"};
    etag_file << res.substr(pos + 6);
    std::cout << "For hash: " << hash << " \t" << res.substr(pos + 6)
              << std::endl;
    etag_file.close();
  }
  return n;
}

void Fetcher::init_common() {
  CurlGlobal::init();
  curl = curl_easy_init();
  if (!curl) {
    std::cerr << "Could not init easy curl" << std::endl;
    std::exit(1);
  }

  // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Fetcher::write_cb);
  curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);

  this->headers = nullptr;
}

void Fetcher::update_headers(struct curl_slist *headers) {
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
}

MultiFetcher::MultiFetcher() : multi(curl_multi_init()) {}
MultiFetcher::~MultiFetcher() {
  if (multi) {
    curl_multi_cleanup(multi);
  }
  if (headers) {
    curl_slist_free_all(headers);
  }
}

void MultiFetcher::add(const std::string &f_str, size_t hash) {
  fetchers.emplace_back(f_str);
  CURLMcode res = curl_multi_add_handle(multi, fetchers.back().curl);
  if (res != CURLM_OK) {
    std::cerr << "curl_multi_add_handle() failed, code " << res << std::endl;
    std::cerr << std::string(curl_multi_strerror(res)) << std::endl;
  }
  this->hash.push_back(hash);
}

void MultiFetcher::add(const std::string &f_str) {
  fetchers.emplace_back(f_str);
  CURLMcode res = curl_multi_add_handle(multi, fetchers.back().curl);
  if (res != CURLM_OK) {
    std::cerr << "curl_multi_add_handle() failed, code " << res << std::endl;
    std::cerr << std::string(curl_multi_strerror(res)) << std::endl;
  }
  this->hash.push_back(std::hash<std::string>{}(f_str));
}

void MultiFetcher::add(Fetcher &&f) {
  fetchers.push_back(std::move(f));
  CURLMcode res = curl_multi_add_handle(multi, fetchers.back().curl);
  if (res != CURLM_OK) {
    std::cerr << "curl_multi_add_handle() failed, code " << res << std::endl;
    std::cerr << std::string(curl_multi_strerror(res)) << std::endl;
  }
  this->hash.push_back(std::hash<std::string>{}(f.url));
}

bool MultiFetcher::perform_write() {
  int still_running;
  std::vector<FILE *> fps{};
  fps.reserve(fetchers.size());

  for (size_t h : hash) {
    std::filesystem::path url_dir{DefeedCtx::rss + "/" + std::to_string(h)};
    if (!std::filesystem::is_directory(url_dir)) {
      if (!std::filesystem::create_directory(url_dir)) {
        std::cerr << "Unable to mkdir " << url_dir.string() << std::endl;
        std::abort();
      }
    }
  }

  for (size_t i = 0; i < fetchers.size(); i++) {
    std::string fname{DefeedCtx::rss + "/" + std::to_string(hash[i]) +
                      "/rssfeed.txt"};
    FILE *pagefile = override_file ? std::fopen(fname.c_str(), "wb")
                                   : std::fopen(fname.c_str(), "ab");
    if (!pagefile) {
      std::cerr << "Unable to open file: " << fname << std::endl;
      return false;
    }
    curl_easy_setopt(fetchers[i].curl, CURLOPT_WRITEDATA, pagefile);
    fps.push_back(pagefile);
    curl_easy_setopt(fetchers[i].curl, CURLOPT_HEADERDATA, &hash[i]);
  }

  do {
    CURLMcode mresult = curl_multi_perform(multi, &still_running);
    if (mresult != CURLM_OK) {
      std::cerr << "curl_multi_perform() failed, code" << mresult << std::endl;
      break;
    }

    mresult = curl_multi_poll(multi, NULL, 0, 1000, NULL);
    if (mresult != CURLM_OK) {
      std::cerr << "curl_multi_poll() failed, code " << mresult << std::endl;
      break;
    }
  } while (still_running);

  for (size_t i = 0; i < fetchers.size(); i++) {
    fclose(fps[i]);
  }

  return true;
}

void MultiFetcher ::append_headers(const std::string &header_str) {
  this->headers = curl_slist_append(headers, header_str.c_str());
}

void MultiFetcher::update_fetcher_headers() {
  for (Fetcher &fetcher : fetchers) {
    fetcher.update_headers(headers);
  }
}

void MultiFetcher::reset_override_file() { override_file = false; }
