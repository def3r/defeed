#include <iostream>

#include <curl/curl.h>

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
