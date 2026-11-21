#pragma once

#include <iostream>

#include <curl/curl.h>

struct CurlGlobal {
  static inline void init() {
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
  };

  static inline void cleanup() {
    static bool ccleanup = false;
    if (ccleanup == true) {
      return;
    }
    curl_global_cleanup();
    ccleanup = true;
  }

private:
  inline CurlGlobal() {};
};
