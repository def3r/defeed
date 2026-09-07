#include <filesystem>
#include <fstream>
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
