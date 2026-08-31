#include <fstream>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <iostream>
#include <string>

#include <curl/curl.h>

class CurlGlobal {
private:
  CurlGlobal() {}

public:
  static void init() {
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

  static void cleanup() {
    static bool ccleanup = false;
    if (ccleanup == true) {
      return;
    }
    curl_global_cleanup();
    ccleanup = true;
  }
};

class Fetcher {
private:
  CURL *curl;
  CURLcode res;

  static size_t write_cb(char *ptr, size_t size, size_t nmemb, void *stream) {
    size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
    return written;
  }

  void init_common() {
    CurlGlobal::init();
    curl = curl_easy_init();
    if (!curl) {
      std::cerr << "Could not init easy curl" << std::endl;
      std::exit(1);
    }
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Fetcher::write_cb);
  }

public:
  Fetcher() { init_common(); }

  Fetcher(const std::string &url) {
    init_common();
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  }

  ~Fetcher() { curl_easy_cleanup(curl); }

  CURLcode write_file(const std::string &fname) {
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
};

int main(int argc, char *argv[]) {
  using namespace ftxui;

  std::string rss_file = "rss.txt";

  if (argc > 1) {
    rss_file = argv[1];
  }

  std::ifstream rss_feed_file(rss_file);
  if (!rss_feed_file.is_open()) {
    std::cerr << "Cant open file " << rss_file << std::endl;
    std::exit(1);
  }

  int i = 0;
  while (!rss_feed_file.eof()) {
    std::string line{};
    std::string outname{"outfile" + std::to_string(i)};
    std::getline(rss_feed_file, line);
    if (!line.empty()) {
      std::cout << line << std::endl;
      Fetcher f(line);
      CURLcode res = f.write_file(outname);
      std::cout << "Res=" << res << std::endl;
    }
    i++;
  }

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
