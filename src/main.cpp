#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include <curl/curl.h>
#include <curl/multi.h>

#include "fetcher.h"
#include "global.h"

// TODO: curl_multi: https://curl.se/libcurl/c/libcurl-multi.html

void defeed_setup() {
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

int main(int argc, char *argv[]) {
  using namespace ftxui;

  DefeedCtx::init();
  defeed_setup();

  std::ifstream rss_info{DefeedCtx::rss_info};
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
      std::cout << url << "\t\t" << std::hash<std::string>{}(url) << std::endl;
      rss_urls.push_back({url, std::hash<std::string>{}(url)});
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
      std::cout << "Can't find etag file for " << hash << " (" << url << ")"
                << std::endl;
      fetch_rss.add(url);
      continue;
    }
    std::string etag{};
    etag_file >> etag;
    etag_file.close();

    Fetcher f{url};
    f.append_headers("If-None-Match: " + etag);
    f.apply_headers();
    cond_fetch_rss.add(std::move(f));
  }

  fetch_rss.perform_write();
  cond_fetch_rss.perform_write();

  // Create a simple document with three text elements.
  Element document = hbox({
      text("left") | border,
      text("middle") | border | flex,
      text("right") | border,
  });

  // Create a screen with full width and height fitting the document.
  auto screen = Screen::Create(Dimension::Full(),       // Width
                               Dimension::Fit(document) // Height
  );

  // Render the document onto the screen.
  Render(screen, document);

  // Print the screen to the console.
  screen.Print();

  CurlGlobal::cleanup();
}
