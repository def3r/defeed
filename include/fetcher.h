#pragma once

#include <string>
#include <vector>

#include <curl/curl.h>

#include "global.h"

class MultiFetcher;

class Fetcher {
  friend class MultiFetcher;

public:
  Fetcher();
  Fetcher(Fetcher &&other) noexcept;
  Fetcher &operator=(Fetcher &&other) noexcept;
  Fetcher(const Fetcher &) = delete;
  Fetcher &operator=(const Fetcher &) = delete;
  Fetcher(const std::string &url);
  ~Fetcher();

  CURLcode perform_write(const std::string &fname);
  void append_headers(const std::string &header_str);
  void apply_headers();

private:
  CURL *curl;
  CURLcode res;
  std::string url;
  std::string url_hash;
  struct curl_slist *headers;

  static size_t write_cb(char *ptr, size_t size, size_t nmemb, void *stream);
  static size_t header_callback(char *buffer, size_t size, size_t nitems,
                                void *userdata);
  void init_common();
  void update_headers(struct curl_slist *headers);
};

class MultiFetcher {
public:
  MultiFetcher();
  ~MultiFetcher();

  void add(const std::string &f_str, size_t hash);
  void add(const std::string &f_str);
  void add(Fetcher &&f);

  bool perform_write();
  void append_headers(const std::string &header_str);
  void update_fetcher_headers();
  void reset_override_file();

private:
  CURLM *multi;
  std::vector<Fetcher> fetchers;
  std::vector<size_t> hash;
  struct curl_slist *headers = NULL;
  bool override_file = true;
};
