#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <curl/curl.h>

#include "fetcher.h"
#include "global.h"

struct UserData {
  size_t hash;
  std::string etag;
};

// BUG: How do we set url, etag and url_hash in this case?
Fetcher::Fetcher() { init_common(); }

// RAII obj, steal on move, delete on copy
// move constructor
Fetcher::Fetcher(Fetcher &&other) noexcept {
  this->curl = other.curl;
  this->url = other.url;
  this->url_hash = other.url_hash;
  this->headers = other.headers;
  this->etag = other.etag;

  other.curl = nullptr;
  other.headers = nullptr;
}

// move assignment
Fetcher &Fetcher::operator=(Fetcher &&other) noexcept {
  if (this != &other) {
    this->curl = other.curl;
    this->url = other.url;
    this->url_hash = other.url_hash;
    this->headers = other.headers;
    this->etag = other.etag;

    other.curl = nullptr;
    other.headers = nullptr;
  }
  return *this;
}

Fetcher::Fetcher(const std::string &url)
    : url(url), url_hash(std::hash<std::string>{}(url)) {
  init_common();
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
}

Fetcher::Fetcher(const std::string &url, const std::string &etag)
    : url(url), url_hash(std::hash<std::string>{}(url)), etag(etag) {
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
  UserData *ud = static_cast<UserData *>(userdata);
  if (ud->hash == 0) {
    // Already recieved a 304 Not Modified; skip
    return n;
  }

  char temp[n - 1]; // ignore \r\n
  std::memcpy(temp, buffer, n - 2);
  temp[n - 2] = '\0';
  std::string res{temp};

  size_t http = res.find("HTTP");
  if (http != std::string::npos) {
    size_t second = res.find_first_of(" ");
    // if we cant find the code, something went terribly wrong
    if (second == std::string::npos) {
      std::cerr << "Cant find status code in the header: " << res << std::endl;
      std::abort();
    }
    // feed 304 Not Modified; early exit
    if (res.substr(second + 1, 3) == "304") {
      std::cout << "hash\t" << ud->hash << "\t304 Not Modified" << std::endl;
      ud->hash = 0;
      return n;
    }
  }

  size_t pos = res.find("etag: ");
  pos = (pos == std::string::npos) ? res.find("ETag: ") : pos;
  if (pos != std::string::npos) {
    std::filesystem::path url_path{DefeedCtx::rss + "/" +
                                   std::to_string(ud->hash)};
    std::string new_etag = res.substr(pos + 6);

    // Sometimes it may happen that server returns 200 OK even tho the etags
    // match because it may happen that CDN may have a cache miss and it ignores
    // the 'If-None-Match' thing in the header
    if (new_etag == ud->etag) {
      std::cout << "Returned 200 but etag not modified for " << ud->hash
                << std::endl;
      ud->hash = 0;
      return n;
    }

    // At this point we assume all the dirs have been setup
    if (!std::filesystem::is_directory(url_path)) {
      std::abort();
    }
    std::ofstream etag_file{url_path.string() + "/etag"};
    etag_file << new_etag;
    std::cout << "ETag modified for hash: " << ud->hash
              << "\t new ETag: " << new_etag << std::endl;
    std::cout << "Fetching rss feed for " << ud->hash << std::endl;
    etag_file.close();
  } else if ((pos = res.find("last")) != std::string::npos) {
    // TODO: fallback to Last-Modified: field
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

  std::vector<UserData> ud;
  ud.reserve(fetchers.size());

  for (size_t i = 0; i < fetchers.size(); i++) {
    std::string fname{DefeedCtx::rss + "/" + std::to_string(hash[i]) +
                      "/new_rssfeed.txt"};
    FILE *pagefile = std::fopen(fname.c_str(), "wb");
    if (!pagefile) {
      std::cerr << "Unable to open file: " << fname << std::endl;
      return false;
    }
    curl_easy_setopt(fetchers[i].curl, CURLOPT_WRITEDATA, pagefile);
    fps.push_back(pagefile);
    ud.emplace_back((UserData){hash[i], fetchers[i].etag});
    curl_easy_setopt(fetchers[i].curl, CURLOPT_HEADERDATA, &ud.back());
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
    long pos = std::ftell(fps[i]);
    std::fclose(fps[i]);
    std::string dname{DefeedCtx::rss + "/" +
                      std::to_string(fetchers[i].url_hash)};
    std::string mv_name{dname + "/new_rssfeed.txt"};
    if (pos != 0) {
      std::string rm_name{dname + "/rssfeed.txt"};
      if (std::remove(rm_name.c_str()) != 0) {
        std::cerr << "Unable to rm file: " << rm_name << std::endl;
      }
      if (std::rename(mv_name.c_str(), rm_name.c_str()) != 0) {
        std::cerr << "Unable to rename file: " << mv_name << " to " << rm_name
                  << std::endl;
      }
    } else if (std::remove(mv_name.c_str()) != 0) {
      std::cerr << "Unable to rm file: " << mv_name << std::endl;
    }
  }

  return true;
}

void MultiFetcher::append_headers(const std::string &header_str) {
  this->headers = curl_slist_append(headers, header_str.c_str());
}

void MultiFetcher::update_fetcher_headers() {
  for (Fetcher &fetcher : fetchers) {
    fetcher.update_headers(headers);
  }
}
